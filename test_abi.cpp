#include <stdint.h>
#include <stdio.h>
struct OPENFILENAMEA_x64 {
  uint32_t lStructSize;
  uint32_t padding;
  uint64_t hwndOwner;
  uint64_t hInstance;
  uint64_t lpstrFilter;
  uint64_t lpstrCustomFilter;
  uint32_t nMaxCustFilter;
  uint32_t nFilterIndex;
  char*    lpstrFile;
  uint32_t nMaxFile;
  uint32_t padding2;
};
int main() {
  printf("offset lpstrFile=%lu\n", __builtin_offsetof(OPENFILENAMEA_x64, lpstrFile));
  return 0;
}
