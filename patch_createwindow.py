import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

old_cw = """    if (hWndParent) {
        int parent_slot = ((uint64_t)hWndParent) - 1;
        if (parent_slot >= 0 && parent_slot < 20) {
            int c_idx = windows[parent_slot].num_controls++;"""
new_cw = """    if (hWndParent) {
        int parent_slot = ((uint64_t)hWndParent) - 1;
        if (parent_slot >= 0 && parent_slot < 20) {
            if (windows[parent_slot].num_controls >= 32) {
                printf("[WIN32] WARNING: Max controls (32) reached for window %d! Cannot add more.\\n", parent_slot);
                return 0;
            }
            int c_idx = windows[parent_slot].num_controls++;"""

content = content.replace(old_cw, new_cw)

# Let's also check if IFileOpenDialog is being requested and properly return an error or stub it.
# We don't have its CLSID, but CoCreateInstance already returns E_NOINTERFACE.
# To prevent NULL dereference, CoCreateInstance sets *ppv = 0.
# If they don't check, it crashes. We can't prevent that unless we return a dummy COM object!
# Let's create a dummy COM object for CoCreateInstance that just returns 0 for all methods.

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
