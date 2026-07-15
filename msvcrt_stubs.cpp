#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

extern "C" {

#ifdef __x86_64__
#define WIN_ABI __attribute__((ms_abi))
#else
#define WIN_ABI __attribute__((stdcall))
#endif

WIN_ABI void* meinos_memcpy(void* dest, const void* src, size_t n) { return memcpy(dest, src, n); }
WIN_ABI void* meinos_memset(void* s, int c, size_t n) { return memset(s, c, n); }
WIN_ABI void* meinos_memmove(void* dest, const void* src, size_t n) { return memmove(dest, src, n); }
WIN_ABI int meinos_memcmp(const void* s1, const void* s2, size_t n) { return memcmp(s1, s2, n); }

WIN_ABI void* meinos_malloc(size_t size) { return malloc(size); }
WIN_ABI void meinos_free(void* ptr) { free(ptr); }
WIN_ABI void* meinos_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }
WIN_ABI void* meinos_realloc(void* ptr, size_t size) { return realloc(ptr, size); }

WIN_ABI size_t meinos_strlen(const char* s) { return strlen(s); }
WIN_ABI char* meinos_strchr(const char* s, int c) { return strchr(s, c); }
WIN_ABI int meinos_strcmp(const char* s1, const char* s2) { return strcmp(s1, s2); }
WIN_ABI int meinos_strncmp(const char* s1, const char* s2, size_t n) { return strncmp(s1, s2, n); }

WIN_ABI void meinos_abort() {
    printf("\n[CRT] ABORT CALLED BY APPLICATION!\n");
    while(1);
}

WIN_ABI void meinos_exit(int status) {
    printf("\n[CRT] EXIT CALLED: %d\n", status);
    while(1);
}

WIN_ABI void* meinos_acrt_iob_func(unsigned idx) {
    // 0 = stdin, 1 = stdout, 2 = stderr
    if (idx == 0) return stdin;
    if (idx == 1) return stdout;
    if (idx == 2) return stderr;
    return stdout;
}

WIN_ABI int meinos_stdio_common_vfprintf(uint64_t options, FILE* stream, const char* format, void* locale, void* arglist) {
    printf("[APP PRINTF]: ");
    // Because va_list on Linux is different from MS ABI va_list (which is just a pointer),
    // we cannot safely pass `arglist` to `vfprintf` on x64!
    // But we can print the format string.
    printf("%s\n", format);
    return 0;
}

WIN_ABI void* meinos_C_specific_handler() {
    return 0; // Structured exception handling dummy
}

WIN_ABI int meinos_ismbblead(unsigned int c) {
    return 0; // basic multibyte check dummy
}

} // extern "C"
