with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

idx = content.rfind("#include <string.h>")
if idx != -1:
    content = content[:idx]

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
