#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <asm/prctl.h>

int main() {
    void* teb = malloc(4096);
    memset(teb, 0, 4096);
    long res = syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long)teb);
    printf("Result: %ld\n", res);
    return 0;
}
