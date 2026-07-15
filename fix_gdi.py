import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_gdi = """extern "C" WIN_ABI int meinos_GdipGetImageWidth(MeinosGpImage* image, uint32_t* width) {
    if (image && width) { *width = image->w; return 0; }
    return 2;
}

extern "C" WIN_ABI int meinos_GdipGetImageHeight(MeinosGpImage* image, uint32_t* height) {
    if (image && height) { *height = image->h; return 0; }
    return 2;
}

extern "C" WIN_ABI int meinos_GdipDrawImageRectI(void* graphics, MeinosGpImage* image, int x, int y, int width, int height) {
    if (!graphics || !image) return 2;
    uint64_t slot = (uint64_t)graphics;
    if (slot >= 0 && slot < 20 && windows[slot].pixels) {
        int dst_w = windows[slot].pixel_w;
        int dst_h = windows[slot].pixel_h;
        for (int dy = 0; dy < height; dy++) {
            int sy = (dy * image->h) / height;
            int screen_y = y + dy;
            if (screen_y < 0 || screen_y >= dst_h) continue;
            for (int dx = 0; dx < width; dx++) {
                int sx = (dx * image->w) / width;
                int screen_x = x + dx;
                if (screen_x < 0 || screen_x >= dst_w) continue;
                
                uint32_t src_col = image->pixels[sy * image->w + sx];
                uint8_t a = (src_col >> 24) & 0xFF;
                if (a == 0) continue; // fully transparent
                
                if (a == 255) {
                    windows[slot].pixels[screen_y * dst_w + screen_x] = src_col & 0xFFFFFF;
                } else {
                    uint32_t dst_col = windows[slot].pixels[screen_y * dst_w + screen_x];
                    uint8_t r1 = (src_col >> 16) & 0xFF;
                    uint8_t g1 = (src_col >> 8) & 0xFF;
                    uint8_t b1 = src_col & 0xFF;
                    uint8_t r2 = (dst_col >> 16) & 0xFF;
                    uint8_t g2 = (dst_col >> 8) & 0xFF;
                    uint8_t b2 = dst_col & 0xFF;
                    uint8_t r = (r1 * a + r2 * (255 - a)) / 255;
                    uint8_t g = (g1 * a + g2 * (255 - a)) / 255;
                    uint8_t b = (b1 * a + b2 * (255 - a)) / 255;
                    windows[slot].pixels[screen_y * dst_w + screen_x] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }
    return 0;
}
"""

content = content.replace("extern \"C\" WIN_ABI int meinos_GdipDisposeImage(MeinosGpImage* image) {\n    if (image) {", new_gdi + "\nextern \"C\" WIN_ABI int meinos_GdipDisposeImage(MeinosGpImage* image) {\n    if (image) {")

# Map them
mapping = """        if (str_iequals(func_name, "GdipDrawImageRectI")) return (uint64_t)meinos_GdipDrawImageRectI;
        if (str_iequals(func_name, "GdipGetImageWidth")) return (uint64_t)meinos_GdipGetImageWidth;
        if (str_iequals(func_name, "GdipGetImageHeight")) return (uint64_t)meinos_GdipGetImageHeight;"""

content = content.replace("if (str_iequals(func_name, \"GdipFillRectangleI\")) return (uint64_t)meinos_GdipFillRectangleI;", "if (str_iequals(func_name, \"GdipFillRectangleI\")) return (uint64_t)meinos_GdipFillRectangleI;\n" + mapping)

content = content.replace("if (str_iequals(func_name, \"GdipDisposeImage\")) return (uint64_t)meinos_GdipDisposeImage;", "if (str_iequals(func_name, \"GdipDisposeImage\")) return (uint64_t)meinos_GdipDisposeImage;\n" + mapping)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
