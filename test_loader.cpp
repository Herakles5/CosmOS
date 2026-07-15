#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void memcpy_pe(void* dest, void* src, size_t n) { memcpy(dest, src, n); }
void memset_pe(void* dest, int c, size_t n) { memset(dest, c, n); }

uint64_t resolve_windows_api(char* dll, char* func) { return 0; }

struct PE_IMAGE_SECTION_HEADER {
    char Name[8];
    union { uint32_t PhysicalAddress; uint32_t VirtualSize; };
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
    union { uint32_t Characteristics; uint32_t OriginalFirstThunk; };
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
};

uint64_t load_and_resolve_pe(uint8_t* pe_data, uint32_t size) {
    if (size < 0x40) { printf("fail 1\n"); return 0; }
    if (pe_data[0] != 'M' || pe_data[1] != 'Z') { printf("fail 2\n"); return 0; }

    uint32_t pe_offset = *(uint32_t*)(&pe_data[0x3C]);
    if (pe_offset + 24 >= size) { printf("fail 3\n"); return 0; }

    if (pe_data[pe_offset] != 'P' || pe_data[pe_offset+1] != 'E' ||
        pe_data[pe_offset+2] != 0 || pe_data[pe_offset+3] != 0) { printf("fail 4\n"); return 0; }

    uint16_t num_sections = *(uint16_t*)(&pe_data[pe_offset + 6]);
    uint16_t size_of_opt_header = *(uint16_t*)(&pe_data[pe_offset + 20]);

    uint16_t magic = *(uint16_t*)(&pe_data[pe_offset + 24]);
    uint32_t entry_point_rva = *(uint32_t*)(&pe_data[pe_offset + 40]);
    uint32_t size_of_image = *(uint32_t*)(&pe_data[pe_offset + 80]);

    uint32_t import_dir_rva = 0;
    uint32_t import_dir_size = 0;

    if (magic == 0x010B) { // 32-bit
        import_dir_rva = *(uint32_t*)(&pe_data[pe_offset + 128]);
        import_dir_size = *(uint32_t*)(&pe_data[pe_offset + 132]);
    } else if (magic == 0x020B) { // 64-bit
        import_dir_rva = *(uint32_t*)(&pe_data[pe_offset + 144]);
        import_dir_size = *(uint32_t*)(&pe_data[pe_offset + 148]);
    } else {
        printf("fail 5 (magic %x)\n", magic); return 0;
    }

    uint8_t* image_base_ptr = (uint8_t*)malloc(size_of_image);
    if (!image_base_ptr) { printf("fail 6\n"); return 0; }
    memset_pe(image_base_ptr, 0, size_of_image);

    uint32_t size_of_headers = *(uint32_t*)(&pe_data[pe_offset + 84]);
    memcpy_pe(image_base_ptr, pe_data, size_of_headers);

    PE_IMAGE_SECTION_HEADER* section_headers = (PE_IMAGE_SECTION_HEADER*)(&pe_data[pe_offset + 24 + size_of_opt_header]);
    for (int i = 0; i < num_sections; i++) {
        if (section_headers[i].SizeOfRawData > 0) {
            uint8_t* dest = image_base_ptr + section_headers[i].VirtualAddress;
            uint8_t* src = pe_data + section_headers[i].PointerToRawData;
            memcpy_pe(dest, src, section_headers[i].SizeOfRawData);
        }
    }

    if (import_dir_rva > 0 && import_dir_size > 0) {
        PE_IMAGE_IMPORT_DESCRIPTOR* import_desc = (PE_IMAGE_IMPORT_DESCRIPTOR*)(image_base_ptr + import_dir_rva);

        while (import_desc->Name != 0) {
            char* dll_name = (char*)(image_base_ptr + import_desc->Name);
            uint32_t thunk_rva = import_desc->OriginalFirstThunk ? import_desc->OriginalFirstThunk : import_desc->FirstThunk;
            if (magic == 0x010B) {
                uint32_t* thunk32 = (uint32_t*)(image_base_ptr + thunk_rva);
                uint32_t* iat32 = (uint32_t*)(image_base_ptr + import_desc->FirstThunk);
                int i = 0;
                while (thunk32[i] != 0) {
                    char* func_name = nullptr;
                    if ((thunk32[i] & 0x80000000) == 0) {
                        func_name = (char*)(image_base_ptr + thunk32[i] + 2);
                    }
                    iat32[i] = resolve_windows_api(dll_name, func_name);
                    i++;
                }
            } else {
                uint64_t* thunk64 = (uint64_t*)(image_base_ptr + thunk_rva);
                uint64_t* iat64 = (uint64_t*)(image_base_ptr + import_desc->FirstThunk);
                int i = 0;
                while (thunk64[i] != 0) {
                    char* func_name = nullptr;
                    if ((thunk64[i] & 0x8000000000000000ULL) == 0) {
                        func_name = (char*)(image_base_ptr + (thunk64[i] & 0x7FFFFFFF) + 2);
                    }
                    iat64[i] = resolve_windows_api(dll_name, func_name);
                    i++;
                }
            }
            import_desc++;
        }
    }
    printf("success! entry=%p\n", image_base_ptr + entry_point_rva);
    return (uint64_t)(image_base_ptr + entry_point_rva);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* data = (uint8_t*)malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    load_and_resolve_pe(data, size);
    return 0;
}
