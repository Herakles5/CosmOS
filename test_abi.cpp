#include <stdio.h>
#include <stdint.h>
#define WIN_ABI __attribute__((ms_abi))
extern "C" WIN_ABI void test_func(void* a, void* b) {
    printf("a=%p, b=%p\n", a, b);
}
int main() {
    typedef void (*SysVFunc)(void*, void*, void*, void*);
    SysVFunc f = (SysVFunc)test_func;
    f((void*)1, (void*)2, (void*)3, (void*)4);
    return 0;
}
