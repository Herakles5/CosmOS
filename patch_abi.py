import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

content = content.replace("void* (*start)(void*) = args->start_address;", "void* (*start)(void*) __attribute__((ms_abi)) = args->start_address;")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
