#include <stdlib.h>
extern "C" __attribute__((ms_abi)) void* test_func() {
    __attribute__((aligned(16))) char dummy[16];
    return calloc(1, 10);
}
