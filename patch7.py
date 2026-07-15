with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """
extern "C" WIN_ABI void* meinos_CoTaskMemAlloc(uint64_t cb) {
    return malloc(cb);
}
extern "C" WIN_ABI void meinos_CoTaskMemFree(void* pv) {
    if (pv) free(pv);
}
extern "C" WIN_ABI int meinos_EnableWindow(void* hWnd, int bEnable) {
    return 1;
}
extern "C" WIN_ABI void* meinos_CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, uint32_t bItalic, uint32_t bUnderline, uint32_t bStrikeOut, uint32_t iCharSet, uint32_t iOutPrecision, uint32_t iClipPrecision, uint32_t iQuality, uint32_t iPitchAndFamily, const char* pszFaceName) {
    return (void*)1;
}
extern "C" WIN_ABI void* meinos_GetModuleHandleA(const char* lpModuleName) {
    return (void*)0x400000;
}
extern "C" WIN_ABI void* meinos_GetModuleHandleW(const uint16_t* lpModuleName) {
    return (void*)0x400000;
}
extern "C" WIN_ABI void* meinos_GlobalAlloc(uint32_t uFlags, uint64_t dwBytes) {
    return malloc(dwBytes);
}
extern "C" WIN_ABI void* meinos_GlobalFree(void* hMem) {
    if (hMem) free(hMem);
    return 0;
}
extern "C" WIN_ABI void* meinos_GlobalLock(void* hMem) {
    return hMem;
}
extern "C" WIN_ABI int meinos_GlobalUnlock(void* hMem) {
    return 1;
}
extern "C" WIN_ABI int meinos_CoCreateInstance(void* rclsid, void* pUnkOuter, uint32_t dwClsContext, void* riid, void** ppv) {
    if (ppv) *ppv = 0;
    return 0x80004002; // E_NOINTERFACE
}
extern "C" WIN_ABI int meinos_CoInitializeEx(void* pvReserved, uint32_t dwCoInit) {
    return 0; // S_OK
}
extern "C" WIN_ABI int meinos_CoInitializeSecurity(void* pSecDesc, int cAuthSvc, void* asAuthSvc, void* pReserved1, uint32_t dwAuthnLevel, uint32_t dwImpLevel, void* pAuthList, uint32_t dwCapabilities, void* pReserved3) {
    return 0; // S_OK
}
"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "CoTaskMemAlloc")) return (uint64_t)meinos_CoTaskMemAlloc;
        if (str_iequals(func_name, "CoTaskMemFree")) return (uint64_t)meinos_CoTaskMemFree;
        if (str_iequals(func_name, "EnableWindow")) return (uint64_t)meinos_EnableWindow;
        if (str_iequals(func_name, "CreateFontA")) return (uint64_t)meinos_CreateFontA;
        if (str_iequals(func_name, "GetModuleHandleA")) return (uint64_t)meinos_GetModuleHandleA;
        if (str_iequals(func_name, "GetModuleHandleW")) return (uint64_t)meinos_GetModuleHandleW;
        if (str_iequals(func_name, "GlobalAlloc")) return (uint64_t)meinos_GlobalAlloc;
        if (str_iequals(func_name, "GlobalFree")) return (uint64_t)meinos_GlobalFree;
        if (str_iequals(func_name, "GlobalLock")) return (uint64_t)meinos_GlobalLock;
        if (str_iequals(func_name, "GlobalUnlock")) return (uint64_t)meinos_GlobalUnlock;
        if (str_iequals(func_name, "CoCreateInstance")) return (uint64_t)meinos_CoCreateInstance;
        if (str_iequals(func_name, "CoInitializeEx")) return (uint64_t)meinos_CoInitializeEx;
        if (str_iequals(func_name, "CoInitializeSecurity")) return (uint64_t)meinos_CoInitializeSecurity;
"""

replacement = mappings + "\n    uint64_t base = load_windows_dll(dll_name);"
content = content.replace("uint64_t base = load_windows_dll(dll_name);", replacement)

# Patch the start_point to use sigsetjmp
old_start = """    in_exe_thread = true;
    start_point();
    in_exe_thread = false;"""

new_start = """    in_exe_thread = true;
    if (sigsetjmp(jump_env, 1) == 0) {
        start_point();
    } else {
        printf("[SYS] MAIN EXE THREAD TERMINATED SAFELY.\\n");
    }
    in_exe_thread = false;"""

content = content.replace(old_start, new_start)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
