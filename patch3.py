with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """
extern "C" WIN_ABI void meinos_endthreadex(uint32_t retval) {
    pthread_exit((void*)(uintptr_t)retval);
}

extern "C" WIN_ABI int meinos_EnumChildWindows(uint64_t hWndParent, void* lpEnumFunc, uint64_t lParam) {
    return 1;
}

extern "C" WIN_ABI void meinos_InitializeCriticalSection(void* lpCriticalSection) {
    // Zero out the CRITICAL_SECTION struct to prevent garbage from causing crashes in Enter/Leave
    if (lpCriticalSection) memset(lpCriticalSection, 0, 40);
}

extern "C" WIN_ABI void meinos_EnterCriticalSection(void* lpCriticalSection) {
}

extern "C" WIN_ABI void meinos_LeaveCriticalSection(void* lpCriticalSection) {
}

extern "C" WIN_ABI void meinos_DeleteCriticalSection(void* lpCriticalSection) {
}

extern "C" WIN_ABI uint64_t meinos_fread(void* ptr, uint64_t size, uint64_t count, void* stream) {
    return fread(ptr, size, count, (FILE*)stream);
}

extern "C" WIN_ABI int meinos_fclose(void* stream) {
    return fclose((FILE*)stream);
}

extern "C" WIN_ABI int meinos_fseek(void* stream, long offset, int origin) {
    return fseek((FILE*)stream, offset, origin);
}

extern "C" WIN_ABI long meinos_ftell(void* stream) {
    return ftell((FILE*)stream);
}

extern "C" WIN_ABI int meinos_AdjustTokenPrivileges(void* TokenHandle, int DisableAllPrivileges, void* NewState, uint32_t BufferLength, void* PreviousState, uint32_t* ReturnLength) {
    return 1;
}

extern "C" WIN_ABI void meinos_Sleep(uint32_t dwMilliseconds) {
    usleep(dwMilliseconds * 1000);
}
"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "_endthreadex")) return (uint64_t)meinos_endthreadex;
        if (str_iequals(func_name, "EnumChildWindows")) return (uint64_t)meinos_EnumChildWindows;
        if (str_iequals(func_name, "InitializeCriticalSection")) return (uint64_t)meinos_InitializeCriticalSection;
        if (str_iequals(func_name, "EnterCriticalSection")) return (uint64_t)meinos_EnterCriticalSection;
        if (str_iequals(func_name, "LeaveCriticalSection")) return (uint64_t)meinos_LeaveCriticalSection;
        if (str_iequals(func_name, "DeleteCriticalSection")) return (uint64_t)meinos_DeleteCriticalSection;
        if (str_iequals(func_name, "fread")) return (uint64_t)meinos_fread;
        if (str_iequals(func_name, "fclose")) return (uint64_t)meinos_fclose;
        if (str_iequals(func_name, "fseek")) return (uint64_t)meinos_fseek;
        if (str_iequals(func_name, "ftell")) return (uint64_t)meinos_ftell;
        if (str_iequals(func_name, "AdjustTokenPrivileges")) return (uint64_t)meinos_AdjustTokenPrivileges;
        if (str_iequals(func_name, "Sleep")) return (uint64_t)meinos_Sleep;
"""

replacement = mappings + "\n    uint64_t base = load_windows_dll(dll_name);"
content = content.replace("uint64_t base = load_windows_dll(dll_name);", replacement)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
