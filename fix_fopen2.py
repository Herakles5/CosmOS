with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

target = """    if (path[0] == 'Z' && path[1] == ':' && path[2] == '\\') {
        final_path = path + 3;
    }
    // Convert backward slashes to forward slashes
    for (int i=0; final_path[i]; i++) {
        if (final_path[i] == '\\') final_path[i] = '/';
    }"""

replacement = """    if (path[0] == 'Z' && path[1] == ':' && path[2] == '\\\\') {
        final_path = path + 3;
    }
    // Convert backward slashes to forward slashes
    for (int i=0; final_path[i]; i++) {
        if (final_path[i] == '\\\\') final_path[i] = '/';
    }"""

content = content.replace(target, replacement)
with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
