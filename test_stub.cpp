#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

const char* global_dummy_name = "Unknown";

extern "C" uint64_t meinos_DummyAPI() {
    printf("[WIN32 DUMMY API CALLED]: %s\n", global_dummy_name);
    return 0;
}

uint64_t generate_dummy_stub(const char* func_name) {
    uint8_t* stub = (uint8_t*)mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    // movabs rax, func_name
    stub[0] = 0x48; stub[1] = 0xB8;
    *(uint64_t*)(&stub[2]) = (uint64_t)func_name;
    
    // movabs r11, &global_dummy_name
    stub[10] = 0x49; stub[11] = 0xBB;
    *(uint64_t*)(&stub[12]) = (uint64_t)&global_dummy_name;
    
    // mov [r11], rax
    stub[20] = 0x49; stub[21] = 0x89; stub[22] = 0x03;
    
    // movabs rax, meinos_DummyAPI
    stub[23] = 0x48; stub[24] = 0xB8;
    *(uint64_t*)(&stub[25]) = (uint64_t)meinos_DummyAPI;
    
    // jmp rax
    stub[33] = 0xFF; stub[34] = 0xE0;
    
    return (uint64_t)stub;
}

int main() {
    uint64_t stub = generate_dummy_stub("MyTestAPI");
    void (*func)() = (void (*)())stub;
    func();
    return 0;
}
