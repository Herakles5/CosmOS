import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

# Add external references
ext_refs = """extern "C" WIN_ABI int my_setjmp(void* jmpbuf);
extern "C" WIN_ABI void my_longjmp(void* jmpbuf, int val);
"""

if "my_setjmp(void* jmpbuf);" not in content:
    content = content.replace("extern \"C\" void Mock_ExitProcess(uint32_t code) {", ext_refs + "extern \"C\" void Mock_ExitProcess(uint32_t code) {")

# Add to msvcrt mapping block
mapping = """        if (str_iequals(func_name, "_setjmp")) return (uint64_t)my_setjmp;
        if (str_iequals(func_name, "longjmp")) return (uint64_t)my_longjmp;
        if (str_iequals(func_name, "_setjmpex")) return (uint64_t)my_setjmp;"""

content = content.replace("if (str_iequals(func_name, \"___mb_cur_max_func\"))", mapping + "\n        if (str_iequals(func_name, \"___mb_cur_max_func\"))")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
