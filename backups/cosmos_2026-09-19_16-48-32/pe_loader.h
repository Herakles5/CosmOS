#ifndef PE_LOADER_H
#define PE_LOADER_H

#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "wow64_trampoline.h"

extern "C" void* malloc(size_t);
// External function that MeinOS must provide to resolve Windows APIs
extern uint64_t resolve_windows_api(const char* dll_name, const char* func_name);

// PE Structures
struct PE_IMAGE_SECTION_HEADER {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

struct PE_IMAGE_IMPORT_DESCRIPTOR {
    uint32_t OriginalFirstThunk;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
};

struct PE_IMAGE_EXPORT_DIRECTORY {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
};

static void memcpy_pe(void* dest, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

static void memset_pe(void* s, uint8_t c, uint64_t n) {
    uint8_t* p = (uint8_t*)s;
    for (uint64_t i = 0; i < n; i++) p[i] = c;
}

// A barebones PE32/PE32+ Header parser (Inspection only)
static bool parse_and_inspect_pe(uint8_t* pe_data, uint32_t size, uint64_t* out_entry_point, uint16_t* out_machine, uint16_t* out_sections) {
    if (size < 0x40) return false;
    
    if (pe_data[0] != 'M' || pe_data[1] != 'Z') return false;
    
    uint32_t pe_offset = *(uint32_t*)(&pe_data[0x3C]);
    if (pe_offset + 24 >= size) return false;
    
    if (pe_data[pe_offset] != 'P' || pe_data[pe_offset+1] != 'E' || 
        pe_data[pe_offset+2] != 0 || pe_data[pe_offset+3] != 0) {
        return false;
    }
    
    uint16_t machine = *(uint16_t*)(&pe_data[pe_offset + 4]);
    uint16_t num_sections = *(uint16_t*)(&pe_data[pe_offset + 6]);
    uint16_t size_of_opt_header = *(uint16_t*)(&pe_data[pe_offset + 20]);
    
    if (out_machine) *out_machine = machine;
    if (out_sections) *out_sections = num_sections;
    
    if (pe_offset + 24 + size_of_opt_header >= size) return false;
    
    uint16_t magic = *(uint16_t*)(&pe_data[pe_offset + 24]);
    uint32_t entry_point_rva = *(uint32_t*)(&pe_data[pe_offset + 40]);
    uint64_t image_base = 0;
    
    if (magic == 0x010B) { 
        image_base = *(uint32_t*)(&pe_data[pe_offset + 52]);
    } else if (magic == 0x020B) { 
        image_base = *(uint64_t*)(&pe_data[pe_offset + 48]);
    } else {
        return false;
    }
    
    if (out_entry_point) *out_entry_point = image_base + entry_point_rva;
    return true;
}

// Extract export from a LOADED (virtual) PE image
static uint64_t get_pe_export(uint8_t* image_base_ptr, const char* target_func) {
    if (!image_base_ptr || !target_func) return 0;
    
    uint32_t pe_offset = *(uint32_t*)(&image_base_ptr[0x3C]);
    uint16_t magic = *(uint16_t*)(&image_base_ptr[pe_offset + 24]);
    
    uint32_t export_dir_rva = 0;
    if (magic == 0x010B) { // 32-bit
        export_dir_rva = *(uint32_t*)(&image_base_ptr[pe_offset + 120]);
    } else if (magic == 0x020B) { // 64-bit
        export_dir_rva = *(uint32_t*)(&image_base_ptr[pe_offset + 136]);
    }
    
    if (export_dir_rva == 0) return 0;
    
    PE_IMAGE_EXPORT_DIRECTORY* exp_dir = (PE_IMAGE_EXPORT_DIRECTORY*)(image_base_ptr + export_dir_rva);
    uint32_t* names = (uint32_t*)(image_base_ptr + exp_dir->AddressOfNames);
    uint16_t* ordinals = (uint16_t*)(image_base_ptr + exp_dir->AddressOfNameOrdinals);
    uint32_t* funcs = (uint32_t*)(image_base_ptr + exp_dir->AddressOfFunctions);
    
    for (uint32_t i = 0; i < exp_dir->NumberOfNames; i++) {
        char* func_name = (char*)(image_base_ptr + names[i]);
        
        // Simple strcmp
        const char* a = func_name;
        const char* b = target_func;
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) {
            uint16_t ord = ordinals[i];
            uint32_t func_rva = funcs[ord];
            return (uint64_t)(image_base_ptr + func_rva);
        }
    }
    return 0;
}

// FULL PE LOADER: Maps memory and resolves IAT (WINE Core)
static uint64_t load_and_resolve_pe(uint8_t* pe_data, uint32_t size) {
    if (size < 0x40) return 0;
    if (pe_data[0] != 'M' || pe_data[1] != 'Z') return 0;
    
    uint32_t pe_offset = *(uint32_t*)(&pe_data[0x3C]);
    if (pe_offset + 24 >= size) return 0; 
    
    if (pe_data[pe_offset] != 'P' || pe_data[pe_offset+1] != 'E' || 
        pe_data[pe_offset+2] != 0 || pe_data[pe_offset+3] != 0) return 0;
    
    uint16_t num_sections = *(uint16_t*)(&pe_data[pe_offset + 6]);
    uint16_t size_of_opt_header = *(uint16_t*)(&pe_data[pe_offset + 20]);
    
    uint16_t magic = *(uint16_t*)(&pe_data[pe_offset + 24]);
    uint32_t entry_point_rva = *(uint32_t*)(&pe_data[pe_offset + 40]);
    uint32_t size_of_image = *(uint32_t*)(&pe_data[pe_offset + 80]);
    
    uint32_t import_dir_rva = 0;
    uint32_t import_dir_size = 0;
    uint64_t image_base = 0;

    extern bool is_pe_32bit;
    if (magic == 0x010B) { // 32-bit PE32
        is_pe_32bit = true;
        import_dir_rva = *(uint32_t*)(&pe_data[pe_offset + 128]);
        import_dir_size = *(uint32_t*)(&pe_data[pe_offset + 132]);
        image_base = *(uint32_t*)(&pe_data[pe_offset + 52]);
    } else if (magic == 0x020B) { // 64-bit PE32+
        is_pe_32bit = false;
        import_dir_rva = *(uint32_t*)(&pe_data[pe_offset + 144]);
        import_dir_size = *(uint32_t*)(&pe_data[pe_offset + 148]);
        image_base = *(uint64_t*)(&pe_data[pe_offset + 48]);
    } else {
        return 0;
    }
    
    PE_IMAGE_SECTION_HEADER* section_headers = (PE_IMAGE_SECTION_HEADER*)(&pe_data[pe_offset + 24 + size_of_opt_header]);
    uint32_t calc_image_size = size_of_image;
    for (int i = 0; i < num_sections; i++) {
        char sec_name[9] = {0};
        memcpy(sec_name, section_headers[i].Name, 8);
        uint32_t sec_size = section_headers[i].VirtualSize ? section_headers[i].VirtualSize : section_headers[i].SizeOfRawData;
        uint32_t sec_end = section_headers[i].VirtualAddress + sec_size;
        FILE* logf = fopen("pe_loader.log", "a");
        if (logf) {
            fprintf(logf, "[PE SECTION %d] %s: VA=0x%X, VSiz=0x%X, RawSiz=0x%X\n",
                   i, sec_name, section_headers[i].VirtualAddress, section_headers[i].VirtualSize,
                   section_headers[i].SizeOfRawData);
            fclose(logf);
        }
        if (sec_end > calc_image_size) {
            calc_image_size = sec_end;
        }
    }
    // Add 1MB padding for BSS and uninitialized memory
    calc_image_size += 0x100000;
    calc_image_size = (calc_image_size + 0xFFF) & ~0xFFF;
    size_of_image = calc_image_size;
    
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

    // Allocate memory for the virtual image, try to allocate at preferred ImageBase
    int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_32BIT
    if (magic == 0x010B) mmap_flags |= MAP_32BIT;
#endif
    uint8_t* image_base_ptr = (uint8_t*)mmap((void*)image_base, size_of_image, PROT_READ|PROT_WRITE|PROT_EXEC, mmap_flags | MAP_FIXED_NOREPLACE, -1, 0);
    if (image_base_ptr == MAP_FAILED || image_base_ptr != (uint8_t*)image_base) {
        image_base_ptr = (uint8_t*)mmap((void*)image_base, size_of_image, PROT_READ|PROT_WRITE|PROT_EXEC, mmap_flags | MAP_FIXED, -1, 0);
    }
    if (image_base_ptr == MAP_FAILED) {
        // Fallback to anywhere
        image_base_ptr = (uint8_t*)mmap(NULL, size_of_image, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    }
    if (image_base_ptr == MAP_FAILED || !image_base_ptr) return 0;
    
    uint64_t delta = (uint64_t)image_base_ptr - image_base;
    FILE* logf = fopen("pe_loader.log", "a");
    if (logf) {
        fprintf(logf, "[PE LOADER] Base: 0x%llX, Map: 0x%llX (Size: 0x%X)\n",
               (unsigned long long)image_base, (unsigned long long)image_base_ptr, size_of_image);
        fclose(logf);
    }
    memset_pe(image_base_ptr, 0, size_of_image);
    
    // Copy Headers
    uint32_t size_of_headers = *(uint32_t*)(&pe_data[pe_offset + 84]);
    memcpy_pe(image_base_ptr, pe_data, size_of_headers);
    
    // Copy Sections to their VirtualAddress
    for (int i = 0; i < num_sections; i++) {
        if (section_headers[i].SizeOfRawData > 0) {
            uint8_t* dest = image_base_ptr + section_headers[i].VirtualAddress;
            uint8_t* src = pe_data + section_headers[i].PointerToRawData;
            memcpy_pe(dest, src, section_headers[i].SizeOfRawData);
        }
    }
    
    uint32_t ptr_before_iat = *(uint32_t*)(image_base_ptr + 0x4A0E8);
    printf("[SYS] POINTER AT 0x44A0E8 (BEFORE IAT): 0x%X\n", ptr_before_iat);

    // Resolve Imports (IAT)
    if (import_dir_rva > 0 && import_dir_size > 0) {
        PE_IMAGE_IMPORT_DESCRIPTOR* import_desc = (PE_IMAGE_IMPORT_DESCRIPTOR*)(image_base_ptr + import_dir_rva);
        
        while (import_desc->Name != 0) {
            char* dll_name = (char*)(image_base_ptr + import_desc->Name);
            
            uint32_t thunk_rva = import_desc->OriginalFirstThunk ? import_desc->OriginalFirstThunk : import_desc->FirstThunk;
            
            if (magic == 0x010B) { // 32-bit IAT
                uint32_t* thunk32 = (uint32_t*)(image_base_ptr + thunk_rva);
                uint32_t* iat32 = (uint32_t*)(image_base_ptr + import_desc->FirstThunk);
                int i = 0;
                while (thunk32[i] != 0) {
                    char* func_name = nullptr; // nullptr means Ordinal import
                    if ((thunk32[i] & 0x80000000) == 0) {
                        func_name = (char*)(image_base_ptr + thunk32[i] + 2);
                    }
                    uint64_t resolved_addr = resolve_windows_api(dll_name, func_name);
                    
                    // We need to keep track of the API mapping for the WoW64 dispatcher.
                    extern uint64_t wow64_api_table[4096];
                    extern int wow64_api_table_count;
                    extern uint32_t wow64_arg_bytes_table[16384];
                    extern char* wow64_api_name_table[16384];
                    uint32_t get_api_arg_bytes(const char* name);
                    
                    int api_id = wow64_api_table_count++;
                    wow64_api_table[api_id] = resolved_addr;
                    wow64_api_name_table[api_id] = strdup(func_name ? func_name : "Ordinal");
                    wow64_arg_bytes_table[api_id] = get_api_arg_bytes(func_name);

                    // Print IAT entry resolution ONLY for 0x4A0E8!
                    uint32_t current_iat_rva = import_desc->FirstThunk + i * 4;
                    if (current_iat_rva == 0x4A0E8) {
                        printf("[SYS] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                        printf("[SYS] FOUND CRASH IAT: 0x4A0E8 -> %s::%s\n", dll_name, func_name ? func_name : "Ordinal");
                        printf("[SYS] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                    }
                    
                    // The 32-bit program jumps to wow64_api_thunk with api_id in EAX
                    uint32_t thunk_addr = generate_wow64_thunk(api_id);
                    iat32[i] = thunk_addr;
                    i++;
                }
            } else { // 64-bit IAT
                uint64_t* thunk64 = (uint64_t*)(image_base_ptr + thunk_rva);
                uint64_t* iat64 = (uint64_t*)(image_base_ptr + import_desc->FirstThunk);
                int i = 0;
                while (thunk64[i] != 0) {
                    char* func_name = nullptr;
                    if ((thunk64[i] & 0x8000000000000000ULL) == 0) {
                        func_name = (char*)(image_base_ptr + (thunk64[i] & 0x7FFFFFFF) + 2);
                    }
                    uint64_t resolved_addr = resolve_windows_api(dll_name, func_name);
                    iat64[i] = resolved_addr;
                    i++;
                }
            }
            import_desc++;
        }
    }

    // Apply Base Relocations if delta != 0
    if (delta != 0) {
        uint32_t reloc_dir_rva = 0;
        uint32_t reloc_dir_size = 0;
        if (magic == 0x010B) {
            reloc_dir_rva = *(uint32_t*)(&pe_data[pe_offset + 160]);
            reloc_dir_size = *(uint32_t*)(&pe_data[pe_offset + 164]);
        } else {
            reloc_dir_rva = *(uint32_t*)(&pe_data[pe_offset + 176]);
            reloc_dir_size = *(uint32_t*)(&pe_data[pe_offset + 180]);
        }

        if (reloc_dir_rva > 0 && reloc_dir_size > 0) {
            uint32_t current_rva = reloc_dir_rva;
            while (current_rva < reloc_dir_rva + reloc_dir_size) {
                uint32_t page_rva = *(uint32_t*)(image_base_ptr + current_rva);
                uint32_t block_size = *(uint32_t*)(image_base_ptr + current_rva + 4);
                if (block_size == 0) break;

                uint16_t* entries = (uint16_t*)(image_base_ptr + current_rva + 8);
                int num_entries = (block_size - 8) / 2;
                for (int i = 0; i < num_entries; i++) {
                    uint16_t type = entries[i] >> 12;
                    uint16_t offset = entries[i] & 0x0FFF;
                    if (type == 3) { // IMAGE_REL_BASED_HIGHLOW (32-bit)
                        uint32_t* target = (uint32_t*)(image_base_ptr + page_rva + offset);
                        *target += (uint32_t)delta;
                    } else if (type == 10) { // IMAGE_REL_BASED_DIR64 (64-bit)
                        uint64_t* target = (uint64_t*)(image_base_ptr + page_rva + offset);
                        *target += delta;
                    }
                }
                current_rva += block_size;
            }
        }
    }
    
    extern uint64_t meinos_main_exe_base;
    if (meinos_main_exe_base == 0) {
        meinos_main_exe_base = (uint64_t)image_base_ptr;
    }
    
    // SCAN FOR CRASH SIGNATURE
    uint8_t sig[] = {0x87, 0x07, 0x8B, 0xC6, 0xEB, 0x0C, 0x6A, 0xFF};
    for (uint32_t i = 0; i < size_of_image - sizeof(sig); i++) {
        if (memcmp(image_base_ptr + i, sig, sizeof(sig)) == 0) {
            printf("[SYS] Found crash signature at RVA: 0x%X\n", i);
            // Find section
            for (int s = 0; s < num_sections; s++) {
                if (i >= section_headers[s].VirtualAddress && 
                    i < section_headers[s].VirtualAddress + section_headers[s].VirtualSize) {
                    char sname[9] = {0};
                    memcpy(sname, section_headers[s].Name, 8);
                    printf("[SYS] Crash signature belongs to section: %s\n", sname);
                }
            }
        }
    }
    
    // Dump the value at 0x4A0E8
    uint32_t ptr_44a0e8 = *(uint32_t*)(image_base_ptr + 0x4A0E8);
    printf("[SYS] POINTER AT 0x44A0E8: 0x%X\n", ptr_44a0e8);
    if (ptr_44a0e8 >= 0x400000 && ptr_44a0e8 < 0x400000 + size_of_image) {
        printf("[SYS]  -> Points inside the image at RVA: 0x%X\n", ptr_44a0e8 - 0x400000);
    } else {
        printf("[SYS]  -> Points outside the image!\n");
    }

    return (uint64_t)(image_base_ptr + entry_point_rva);
}

#endif
