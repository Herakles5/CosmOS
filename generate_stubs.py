with open("linux_stubs2.cpp", "w") as f:
    f.write('#include <stdio.h>\n')
    f.write('#include <stdint.h>\n')
    f.write('struct Window;\n')
    f.write('extern "C" void* get_window_15();\n')
    f.write('void print_win(Window* win, const char* text);\n')
    f.write('#define WIN_ABI __attribute__((ms_abi))\n')
    
    f.write('extern "C" WIN_ABI uint64_t meinos_DummyAPI_Named(const char* name) {\n')
    f.write('    char msg[128];\n')
    f.write('    snprintf(msg, 128, "[WIN32 DUMMY API CALLED]: %s\\n", name);\n')
    f.write('    print_win((Window*)get_window_15(), msg);\n')
    f.write('    return 0;\n')
    f.write('}\n\n')

    for i in range(1000):
        f.write(f'extern "C" WIN_ABI uint64_t meinos_dummy_stub_{i}() {{\n')
        f.write(f'    extern const char* stub_name_{i};\n')
        f.write(f'    return meinos_DummyAPI_Named(stub_name_{i});\n')
        f.write(f'}}\n')

    f.write('extern "C" void* dummy_stub_array[1000] = {\n')
    for i in range(1000):
        f.write(f'    (void*)meinos_dummy_stub_{i},\n')
    f.write('};\n')
    
    f.write('extern "C" const char* stub_names[1000];\n')
    for i in range(1000):
        f.write(f'const char* stub_name_{i} = "Unknown";\n')

    f.write('extern "C" const char** get_stub_name_ptr(int i) {\n')
    f.write('    static const char** ptrs[1000] = {\n')
    for i in range(1000):
        f.write(f'        &stub_name_{i},\n')
    f.write('    };\n')
    f.write('    return ptrs[i];\n')
    f.write('}\n')
