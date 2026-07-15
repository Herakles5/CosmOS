import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_open = """extern "C" WIN_ABI int meinos_GetOpenFileNameA(void* lpofn) {
    return 0; // Return 0 to simulate user cancelling the dialog. Avoids fake file crash!
}"""

content = re.sub(r'extern "C" WIN_ABI int meinos_GetOpenFileNameA\(void\* lpofn\) \{.*?\n\}', new_open, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
