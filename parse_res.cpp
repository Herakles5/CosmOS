#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

struct PE_RESOURCE_DIR {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint16_t NumberOfNamedEntries;
    uint16_t NumberOfIdEntries;
};

struct PE_RESOURCE_DIR_ENTRY {
    uint32_t Name;
    uint32_t OffsetToData;
};

struct PE_RESOURCE_DATA_ENTRY {
    uint32_t OffsetToData;
    uint32_t Size;
    uint32_t CodePage;
    uint32_t Reserved;
};

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) return 1;
    off_t size = lseek(fd, 0, SEEK_END);
    uint8_t* map = (uint8_t*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    
    // Find PE header
    uint32_t pe_offset = *(uint32_t*)(map + 0x3C);
    uint16_t magic = *(uint16_t*)(map + pe_offset + 24);
    
    uint32_t res_rva = 0;
    uint32_t res_size = 0;
    if (magic == 0x010B) { // 32-bit
        res_rva = *(uint32_t*)(map + pe_offset + 120 + 2*8);
        res_size = *(uint32_t*)(map + pe_offset + 120 + 2*8 + 4);
    } else { // 64-bit
        res_rva = *(uint32_t*)(map + pe_offset + 136 + 2*8);
        res_size = *(uint32_t*)(map + pe_offset + 136 + 2*8 + 4);
    }
    
    if (res_rva == 0) {
        printf("No resources found\n");
        return 0;
    }
    
    // Convert RVA to physical offset (quick and dirty)
    uint32_t num_sections = *(uint16_t*)(map + pe_offset + 6);
    uint32_t opt_hdr_sz = *(uint16_t*)(map + pe_offset + 20);
    uint8_t* sections = map + pe_offset + 24 + opt_hdr_sz;
    
    uint32_t res_file_offset = 0;
    for(uint32_t i=0; i<num_sections; i++) {
        uint32_t va = *(uint32_t*)(sections + i*40 + 12);
        uint32_t vs = *(uint32_t*)(sections + i*40 + 8);
        uint32_t ptr = *(uint32_t*)(sections + i*40 + 20);
        if (res_rva >= va && res_rva < va + vs) {
            res_file_offset = ptr + (res_rva - va);
            break;
        }
    }
    
    printf("Resource RVA: %X, File Offset: %X\n", res_rva, res_file_offset);
    
    uint8_t* root = map + res_file_offset;
    PE_RESOURCE_DIR* dir = (PE_RESOURCE_DIR*)root;
    printf("Root Entries: %d named, %d ID\n", dir->NumberOfNamedEntries, dir->NumberOfIdEntries);
    
    PE_RESOURCE_DIR_ENTRY* entries = (PE_RESOURCE_DIR_ENTRY*)(root + sizeof(PE_RESOURCE_DIR));
    for(int i=0; i<dir->NumberOfNamedEntries + dir->NumberOfIdEntries; i++) {
        uint32_t name = entries[i].Name;
        uint32_t offset = entries[i].OffsetToData;
        printf(" Type ID: %X (is_dir: %d)\n", name, (offset >> 31));
        if ((offset >> 31) && name == 10) { // RT_RCDATA
            PE_RESOURCE_DIR* subdir = (PE_RESOURCE_DIR*)(root + (offset & 0x7FFFFFFF));
            PE_RESOURCE_DIR_ENTRY* subentries = (PE_RESOURCE_DIR_ENTRY*)((uint8_t*)subdir + sizeof(PE_RESOURCE_DIR));
            for(int j=0; j<subdir->NumberOfNamedEntries + subdir->NumberOfIdEntries; j++) {
                uint32_t subname = subentries[j].Name;
                uint32_t suboffset = subentries[j].OffsetToData;
                printf("  Resource ID: %X (is_dir: %d)\n", subname, (suboffset >> 31));
                if (suboffset >> 31) {
                    PE_RESOURCE_DIR* leafdir = (PE_RESOURCE_DIR*)(root + (suboffset & 0x7FFFFFFF));
                    PE_RESOURCE_DIR_ENTRY* leafentries = (PE_RESOURCE_DIR_ENTRY*)((uint8_t*)leafdir + sizeof(PE_RESOURCE_DIR));
                    uint32_t leafdata = leafentries[0].OffsetToData;
                    PE_RESOURCE_DATA_ENTRY* data = (PE_RESOURCE_DATA_ENTRY*)(root + (leafdata & 0x7FFFFFFF));
                    printf("   Data RVA: %X, Size: %d\n", data->OffsetToData, data->Size);
                }
            }
        }
    }
    
    return 0;
}
