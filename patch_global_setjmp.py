import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

mapping = """        if (str_iequals(func_name, "_setjmp")) return (uint64_t)my_setjmp;
        if (str_iequals(func_name, "longjmp")) return (uint64_t)my_longjmp;
        if (str_iequals(func_name, "_setjmpex")) return (uint64_t)my_setjmp;
"""

content = content.replace("if (str_iequals(func_name, \"CoUninitialize\")) return (uint64_t)meinos_CoUninitialize;", mapping + "        if (str_iequals(func_name, \"CoUninitialize\")) return (uint64_t)meinos_CoUninitialize;")

# Also map ole32.dll missing functions properly:
content = content.replace("} else if (str_iequals(dll_name, \"comctl32.dll\")) {", "} else if (str_iequals(dll_name, \"ole32.dll\")) {\n        if (str_iequals(func_name, \"CoCreateInstance\")) return (uint64_t)meinos_CoCreateInstance;\n        if (str_iequals(func_name, \"CoInitializeEx\")) return (uint64_t)meinos_CoInitializeEx;\n        if (str_iequals(func_name, \"CoInitializeSecurity\")) return (uint64_t)meinos_CoInitializeSecurity;\n        if (str_iequals(func_name, \"CreateStreamOnHGlobal\")) return (uint64_t)meinos_CreateStreamOnHGlobal;\n        if (str_iequals(func_name, \"CoUninitialize\")) return (uint64_t)meinos_CoUninitialize;\n        if (str_iequals(func_name, \"CoTaskMemAlloc\")) return (uint64_t)meinos_CoTaskMemAlloc;\n        if (str_iequals(func_name, \"CoTaskMemFree\")) return (uint64_t)meinos_CoTaskMemFree;\n    } else if (str_iequals(dll_name, \"comctl32.dll\")) {")

# Also map gdiplus.dll missing functions properly:
content = content.replace("if (str_iequals(func_name, \"GdipFillRectangleI\")) return (uint64_t)meinos_GdipFillRectangleI;", "if (str_iequals(func_name, \"GdipFillRectangleI\")) return (uint64_t)meinos_GdipFillRectangleI;\n        if (str_iequals(func_name, \"GdipAlloc\")) return (uint64_t)meinos_GdipAlloc;\n        if (str_iequals(func_name, \"GdipFree\")) return (uint64_t)meinos_GdipFree;\n        if (str_iequals(func_name, \"GdipCreateBitmapFromStream\")) return (uint64_t)meinos_GdipCreateBitmapFromStream;\n        if (str_iequals(func_name, \"GdipLoadImageFromStream\")) return (uint64_t)meinos_GdipLoadImageFromStream;\n        if (str_iequals(func_name, \"GdipDisposeImage\")) return (uint64_t)meinos_GdipDisposeImage;")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
