import re

with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

# Add meinos_GetHandleInformation, meinos_fopen, meinos_system
new_funcs = """
extern "C" WIN_ABI int meinos_GetHandleInformation(void* hObject, uint32_t* lpdwFlags) {
    if (lpdwFlags) *lpdwFlags = 0;
    return 1; // Success!
}

extern "C" WIN_ABI void* meinos_fopen(const char* filename, const char* mode) {
    return (void*)fopen(filename, mode);
}

extern "C" WIN_ABI int meinos_system(const char* command) {
    return system(command);
}

extern "C" WIN_ABI void* meinos_SHBrowseForFolderA(void* lpbi) {
    return (void*)0x1234; // dummy PIDL
}

extern "C" WIN_ABI int meinos_SHGetPathFromIDListA(void* pidl, char* pszPath) {
    if (pszPath) {
        strncpy(pszPath, "/tmp", 260);
    }
    return 1;
}

"""

content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "GetWindowLongPtrA")) return (uint64_t)meinos_GetWindowLongPtrA;
        if (str_iequals(func_name, "SetWindowLongPtrA")) return (uint64_t)meinos_SetWindowLongPtrA;
        if (str_iequals(func_name, "GetWindowLongA")) return (uint64_t)meinos_GetWindowLongA;
        if (str_iequals(func_name, "SetWindowLongA")) return (uint64_t)meinos_SetWindowLongA;
        if (str_iequals(func_name, "CallWindowProcA")) return (uint64_t)meinos_CallWindowProcA;
        if (str_iequals(func_name, "FindResourceA")) return (uint64_t)meinos_FindResourceA;
        if (str_iequals(func_name, "SizeofResource")) return (uint64_t)meinos_SizeofResource;
        if (str_iequals(func_name, "LoadResource")) return (uint64_t)meinos_LoadResource;
        if (str_iequals(func_name, "LockResource")) return (uint64_t)meinos_LockResource;
        if (str_iequals(func_name, "GetHandleInformation")) return (uint64_t)meinos_GetHandleInformation;
        if (str_iequals(func_name, "fopen")) return (uint64_t)meinos_fopen;
        if (str_iequals(func_name, "system")) return (uint64_t)meinos_system;
        if (str_iequals(func_name, "SHBrowseForFolderA")) return (uint64_t)meinos_SHBrowseForFolderA;
        if (str_iequals(func_name, "SHGetPathFromIDListA")) return (uint64_t)meinos_SHGetPathFromIDListA;
"""

# Insert the mappings globally before the DLL checks!
# find: uint64_t base = load_windows_dll(dll_name);
replacement = mappings + "\n    uint64_t base = load_windows_dll(dll_name);"
content = content.replace("uint64_t base = load_windows_dll(dll_name);", replacement)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
