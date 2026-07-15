import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

old_dw = """extern "C" WIN_ABI int meinos_DestroyWindow(void* hWnd) {
    uint64_t slot = (uint64_t)hWnd;
    if (slot >= 0 && slot < 20) {"""

new_dw = """extern "C" WIN_ABI int meinos_DestroyWindow(void* hWnd) {
    uint64_t slot = (uint64_t)hWnd - 1;
    if (slot >= 0 && slot < 20) {"""

content = content.replace(old_dw, new_dw)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
