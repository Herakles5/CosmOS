import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

content = content.replace("extern \"C\" WIN_ABI int my_setjmp(void* jmpbuf);\nextern \"C\" WIN_ABI void my_longjmp(void* jmpbuf, int val);\n", "")

decl = """
extern "C" WIN_ABI int my_setjmp(void* jmpbuf);
extern "C" WIN_ABI void my_longjmp(void* jmpbuf, int val);
"""

content = content.replace("struct FakeIStream {", decl + "struct FakeIStream {")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
