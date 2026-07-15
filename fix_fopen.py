import re

with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_fopen = """extern "C" WIN_ABI void* meinos_fopen(const char* filename, const char* mode) {
    if (!filename) return 0;
    char path[256];
    strncpy(path, filename, 255);
    path[255] = 0;
    // Strip Z:\\ if present
    char* final_path = path;
    if (path[0] == 'Z' && path[1] == ':' && path[2] == '\\\\') {
        final_path = path + 3;
    }
    // Convert backward slashes to forward slashes
    for (int i=0; final_path[i]; i++) {
        if (final_path[i] == '\\\\') final_path[i] = '/';
    }
    return (void*)fopen(final_path, mode);
}"""

content = re.sub(r'extern "C" WIN_ABI void\* meinos_fopen\(const char\* filename, const char\* mode\) \{.*?\n\}', new_fopen, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
