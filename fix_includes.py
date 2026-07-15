with open("kernel_main_linux.cpp", "r") as f:
    text = f.read()

text = text.replace("#define STB_IMAGE_IMPLEMENTATION\n#include \"stb_image.h\"\n#include <malloc.h>\n", "")
text = "#define STB_IMAGE_IMPLEMENTATION\n#include \"stb_image.h\"\n#include <malloc.h>\n" + text

with open("kernel_main_linux.cpp", "w") as f:
    f.write(text)
