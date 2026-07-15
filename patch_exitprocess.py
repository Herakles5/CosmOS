import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

old_exit = """extern "C" WIN_ABI void meinos_ExitProcess(uint32_t uExitCode) {
    char msg[64];
    snprintf(msg, sizeof(msg), "\\n[WIN32 ExitProcess] Code: %u\\n", uExitCode);
    print_win(&windows[15], msg);
#ifdef __linux__
    exit(0);
#else
    while(1) { } // Hang the thread on bare metal
#endif
}"""

new_exit = """extern "C" WIN_ABI void meinos_ExitProcess(uint32_t uExitCode) {
    char msg[64];
    snprintf(msg, sizeof(msg), "\\n[WIN32 ExitProcess] Code: %u\\n", uExitCode);
    print_win(&windows[15], msg);
    siglongjmp(jump_env, 1);
}"""

content = content.replace(old_exit, new_exit)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
