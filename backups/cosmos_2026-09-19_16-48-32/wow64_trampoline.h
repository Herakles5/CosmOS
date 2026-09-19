#pragma once
#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>

extern "C" void wow64_api_thunk();

static uint8_t* wow64_trampoline_page = nullptr;
static uint32_t wow64_thunk_count = 0;

inline uint32_t generate_wow64_thunk(uint32_t api_id) {
    if (!wow64_trampoline_page) {
        wow64_trampoline_page = (uint8_t*)mmap(NULL, 65536, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
        printf("[SYS] ALLOCATED THUNK MEMORY AT: 0x%X\n", (uint32_t)(uint64_t)wow64_trampoline_page);
        
        // At the start of the page, write the 32-bit to 64-bit far jump trampoline
        // wow32_entry (offset 0): ljmp 0x33:trampoline_64
        wow64_trampoline_page[0] = 0xEA;
        *(uint32_t*)(&wow64_trampoline_page[1]) = (uint32_t)((uint64_t)wow64_trampoline_page + 7);
        *(uint16_t*)(&wow64_trampoline_page[5]) = 0x33;
        
        // trampoline_64 (offset 7): movabs r11, &wow64_api_thunk; jmp r11
        wow64_trampoline_page[7] = 0x49; wow64_trampoline_page[8] = 0xBB;
        *(uint64_t*)(&wow64_trampoline_page[9]) = (uint64_t)&wow64_api_thunk;
        wow64_trampoline_page[17] = 0x41; wow64_trampoline_page[18] = 0xFF; wow64_trampoline_page[19] = 0xE3;
        
        wow64_thunk_count = 2; // start thunks after the 19-byte header (rounded up to 32)
    }
    
    uint32_t thunk_offset = wow64_thunk_count * 16;
    wow64_thunk_count++;
    
    uint8_t* thunk = wow64_trampoline_page + thunk_offset;
    // mov eax, api_id
    thunk[0] = 0xB8;
    *(uint32_t*)(&thunk[1]) = api_id;
    // jmp wow32_entry
    thunk[5] = 0xE9;
    *(uint32_t*)(&thunk[6]) = (uint32_t)((uint64_t)wow64_trampoline_page - (uint64_t)(thunk + 10));
    
    return (uint32_t)(uint64_t)thunk;
}
