import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """
extern "C" WIN_ABI void* meinos_GdipAlloc(uint64_t size) {
    return malloc(size);
}
extern "C" WIN_ABI void meinos_GdipFree(void* ptr) {
    if (ptr) free(ptr);
}
extern "C" WIN_ABI int meinos_CreateStreamOnHGlobal(void* hGlobal, int fDeleteOnRelease, void** ppstm) {
    if (ppstm) *ppstm = 0;
    return 0x80004002; // E_NOINTERFACE for now, prevents crash but might fail gracefully
}
extern "C" WIN_ABI int meinos_OutputDebugStringA(const char* lpOutputString) {
    printf("[WIN32 OutputDebugStringA]: %s\\n", lpOutputString);
    return 0;
}
extern "C" WIN_ABI int meinos_OutputDebugStringW(const uint16_t* lpOutputString) {
    printf("[WIN32 OutputDebugStringW]\\n");
    return 0;
}
extern "C" WIN_ABI void meinos_CoUninitialize() {
}
"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "GdipAlloc")) return (uint64_t)meinos_GdipAlloc;
        if (str_iequals(func_name, "GdipFree")) return (uint64_t)meinos_GdipFree;
        if (str_iequals(func_name, "CreateStreamOnHGlobal")) return (uint64_t)meinos_CreateStreamOnHGlobal;
        if (str_iequals(func_name, "OutputDebugStringA")) return (uint64_t)meinos_OutputDebugStringA;
        if (str_iequals(func_name, "OutputDebugStringW")) return (uint64_t)meinos_OutputDebugStringW;
        if (str_iequals(func_name, "CoUninitialize")) return (uint64_t)meinos_CoUninitialize;
"""

replacement = mappings + "\n    uint64_t base = load_windows_dll(dll_name);"
content = content.replace("uint64_t base = load_windows_dll(dll_name);", replacement)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
