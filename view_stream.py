with open("kernel_main_linux.cpp", "r") as f:
    lines = f.readlines()
for i, line in enumerate(lines):
    if "struct DummyIStream" in line:
        start = i
        break
for i in range(start, len(lines)):
    print(lines[i], end="")
    if "extern \"C\" WIN_ABI int meinos_CreateStreamOnHGlobal" in lines[i]:
        end = i
        break
for i in range(end, end+15):
    print(lines[i], end="")
