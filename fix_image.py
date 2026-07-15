with open("linux_stubs.cpp", "r") as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if line.startswith("unsigned char* decode_image"):
        lines[i] = 'extern "C" ' + line
    elif line.startswith("void free_image(unsigned char*"):
        lines[i] = 'extern "C" ' + line

with open("linux_stubs.cpp", "w") as f:
    f.writelines(lines)
