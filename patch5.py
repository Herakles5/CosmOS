with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """
extern "C" WIN_ABI void* meinos_LoadLibraryExA(const char* lpLibFileName, void* hFile, uint32_t dwFlags) {
    return meinos_LoadLibraryA(lpLibFileName);
}

extern "C" WIN_ABI void* meinos_LoadLibraryW(const uint16_t* lpLibFileName) {
    return meinos_LoadLibraryA("W_STUB");
}

extern "C" WIN_ABI void* meinos_LoadLibraryExW(const uint16_t* lpLibFileName, void* hFile, uint32_t dwFlags) {
    return meinos_LoadLibraryA("W_STUB");
}

extern "C" WIN_ABI int meinos_mb_cur_max_func() {
    return 1;
}

extern "C" WIN_ABI uint32_t meinos_lc_codepage_func() {
    return 1252; // Windows-1252 codepage
}
"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "LoadLibraryExA")) return (uint64_t)meinos_LoadLibraryExA;
        if (str_iequals(func_name, "LoadLibraryW")) return (uint64_t)meinos_LoadLibraryW;
        if (str_iequals(func_name, "LoadLibraryExW")) return (uint64_t)meinos_LoadLibraryExW;
        if (str_iequals(func_name, "___mb_cur_max_func")) return (uint64_t)meinos_mb_cur_max_func;
        if (str_iequals(func_name, "___lc_codepage_func")) return (uint64_t)meinos_lc_codepage_func;
        if (str_iequals(func_name, "CharPrevA")) return (uint64_t)dummy_stub_array[999]; // Return dummy but prevent logging as unmapped? No, just map to dummy directly.
"""

replacement = mappings + "\n    uint64_t base = load_windows_dll(dll_name);"
content = content.replace("uint64_t base = load_windows_dll(dll_name);", replacement)

# Replace pthread_exit with siglongjmp
content = content.replace("pthread_exit((void*)(uintptr_t)retval);", "siglongjmp(jump_env, 1);")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
