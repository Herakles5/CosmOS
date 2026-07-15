import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_funcs = """extern "C" WIN_ABI void* meinos_SHBrowseForFolderA(void* lpbi) {
    return 0; // Cancel
}

extern "C" WIN_ABI void* meinos_SHBrowseForFolderW(void* lpbi) {
    return 0; // Cancel
}

extern "C" WIN_ABI int meinos_SHGetPathFromIDListA(void* pidl, char* pszPath) {
    return 0;
}

extern "C" WIN_ABI int meinos_SHGetPathFromIDListW(void* pidl, void* pszPath) {
    return 0;
}"""

content = content.replace('extern "C" WIN_ABI void* meinos_SHBrowseForFolderA(void* lpbi) {\n    return 0;\n}', new_funcs)

content = content.replace('if (str_iequals(func_name, "SHBrowseForFolderA")) return (uint64_t)meinos_SHBrowseForFolderA;', 'if (str_iequals(func_name, "SHBrowseForFolderA")) return (uint64_t)meinos_SHBrowseForFolderA;\n        if (str_iequals(func_name, "SHBrowseForFolderW")) return (uint64_t)meinos_SHBrowseForFolderW;\n        if (str_iequals(func_name, "SHGetPathFromIDListA")) return (uint64_t)meinos_SHGetPathFromIDListA;\n        if (str_iequals(func_name, "SHGetPathFromIDListW")) return (uint64_t)meinos_SHGetPathFromIDListW;')

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
