#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
typedef void* HWND;
typedef void* HINSTANCE;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef uint32_t DWORD;

typedef struct tagOFNA {
  DWORD         lStructSize;
  HWND          hwndOwner;
  HINSTANCE     hInstance;
  LPCSTR        lpstrFilter;
  LPSTR         lpstrCustomFilter;
  DWORD         nMaxCustFilter;
  DWORD         nFilterIndex;
  LPSTR         lpstrFile;
  DWORD         nMaxFile;
} OPENFILENAMEA;

int main() {
    printf("%zu\n", offsetof(OPENFILENAMEA, lpstrFile));
    return 0;
}
