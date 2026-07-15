with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

# Remove the duplicated meinos_GetHandleInformation that I just added
# It starts with 'extern "C" WIN_ABI int meinos_GetHandleInformation'
import re
content = re.sub(r'extern "C" WIN_ABI int meinos_GetHandleInformation\(void\* hObject, uint32_t\* lpdwFlags\) \{.*?\n\}', '', content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)

