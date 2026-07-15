import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

content = content.replace("uint64_t slot = (uint64_t)hWnd - 1;\n    if (slot >= 0 && slot < 20) {\n        windows[slot].open = false;", "uint64_t slot = (uint64_t)hWnd;\n    if (slot >= 0 && slot < 20) {\n        windows[slot].open = false;")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
