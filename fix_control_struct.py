import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

control_struct = """struct Control {
    int type; // 0=none, 1=button, 2=static, 3=progress, 4=combobox
    int x, y, w, h;
    char text[64];
    int id;
    int state;
    int data;
    uint64_t wndproc;
    uint64_t user_data;
    char items[10][32];
    int num_items;
    int selected_item;
    int is_dropped;
};"""
content = re.sub(r'struct Control \{.*?\};', control_struct, content, flags=re.DOTALL)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
