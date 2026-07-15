import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

old_wc = """    if (cchWideChar < 0) {
        cchWideChar = 0;
        const uint16_t* p = lpWideCharStr;
        while(*p++) cchWideChar++;
    }"""
new_wc = """    if (!lpWideCharStr) return 0;
    if (cchWideChar < 0) {
        cchWideChar = 0;
        const uint16_t* p = lpWideCharStr;
        while(*p++) cchWideChar++;
    }"""

old_mb = """    if (cbMultiByte < 0) {
        cbMultiByte = 0;
        const char* p = lpMultiByteStr;
        while(*p++) cbMultiByte++;
    }"""
new_mb = """    if (!lpMultiByteStr) return 0;
    if (cbMultiByte < 0) {
        cbMultiByte = 0;
        const char* p = lpMultiByteStr;
        while(*p++) cbMultiByte++;
    }"""

content = content.replace(old_wc, new_wc)
content = content.replace(old_mb, new_mb)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
