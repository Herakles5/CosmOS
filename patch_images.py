import re
with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

new_includes = """#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <malloc.h>
"""
content = content.replace("#include <stdint.h>", "#include <stdint.h>\n" + new_includes)

new_funcs = """
struct FakeIStream {
    void* lpVtbl;
    void* hGlobal;
    int ref_count;
};

extern "C" WIN_ABI uint32_t FakeIStream_Release(FakeIStream* stream) {
    stream->ref_count--;
    if (stream->ref_count == 0) {
        free(stream->lpVtbl);
        free(stream);
        return 0;
    }
    return stream->ref_count;
}

extern "C" WIN_ABI int meinos_CreateStreamOnHGlobal(void* hGlobal, int fDeleteOnRelease, void** ppstm) {
    FakeIStream* stream = (FakeIStream*)malloc(sizeof(FakeIStream));
    stream->hGlobal = hGlobal;
    stream->ref_count = 1;
    
    void** vtbl = (void**)malloc(sizeof(void*) * 14);
    for(int i=0; i<14; i++) vtbl[i] = (void*)0; // NULL by default
    vtbl[2] = (void*)FakeIStream_Release; // Release is index 2 in IStreamVtbl
    
    stream->lpVtbl = vtbl;
    if (ppstm) *ppstm = stream;
    return 0; // S_OK
}

struct MeinosGpImage {
    int w, h;
    uint32_t* pixels;
};

extern "C" WIN_ABI int meinos_GdipCreateBitmapFromStream(FakeIStream* stream, void** bitmap) {
    if (!stream || !bitmap) return 2; // InvalidParameter
    
    size_t size = malloc_usable_size(stream->hGlobal);
    if (size == 0) size = 10000000; // fallback
    
    int w, h, channels;
    uint8_t* data = stbi_load_from_memory((const uint8_t*)stream->hGlobal, size, &w, &h, &channels, 4);
    
    if (!data) {
        printf("[STBI] ERROR: Failed to load image from memory.\\n");
        return 3; // OutOfMemory
    }
    
    MeinosGpImage* img = (MeinosGpImage*)malloc(sizeof(MeinosGpImage));
    img->w = w;
    img->h = h;
    img->pixels = (uint32_t*)malloc(w * h * 4);
    
    // Copy and convert RGBA to ARGB/BGRA (stb outputs RGBA)
    // Windows GDI+ usually wants BGRA (0xAARRGGBB in little endian)
    for(int i=0; i<w*h; i++) {
        uint8_t r = data[i*4 + 0];
        uint8_t g = data[i*4 + 1];
        uint8_t b = data[i*4 + 2];
        uint8_t a = data[i*4 + 3];
        img->pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    free(data);
    *bitmap = img;
    printf("[STBI] OK: Loaded %dx%d image.\\n", w, h);
    return 0; // Ok
}

extern "C" WIN_ABI int meinos_GdipLoadImageFromStream(FakeIStream* stream, void** image) {
    return meinos_GdipCreateBitmapFromStream(stream, image);
}

extern "C" WIN_ABI int meinos_GdipDisposeImage(MeinosGpImage* image) {
    if (image) {
        if (image->pixels) free(image->pixels);
        free(image);
    }
    return 0;
}
"""

content = content.replace("extern \"C\" WIN_ABI int meinos_CreateStreamOnHGlobal", "extern \"C\" WIN_ABI int DUMMY_meinos_CreateStreamOnHGlobal")
content = content.replace("WIN_ABI void meinos_initterm(", new_funcs + "\nWIN_ABI void meinos_initterm(")

mappings = """
        if (str_iequals(func_name, "GdipCreateBitmapFromStream")) return (uint64_t)meinos_GdipCreateBitmapFromStream;
        if (str_iequals(func_name, "GdipLoadImageFromStream")) return (uint64_t)meinos_GdipLoadImageFromStream;
        if (str_iequals(func_name, "GdipDisposeImage")) return (uint64_t)meinos_GdipDisposeImage;
"""
content = content.replace("uint64_t base = load_windows_dll(dll_name);", mappings + "\n    uint64_t base = load_windows_dll(dll_name);")

# Update GdipDrawImageRectI to use our new MeinosGpImage
old_draw = """extern "C" WIN_ABI int meinos_GdipDrawImageRectI(void* graphics, void* image, int x, int y, int width, int height) {
    int parent_slot = -1;
    int c_idx = -1;
    for (int i = 0; i < 20; i++) {
        if (windows[i].active) {
            for (int c = 0; c < windows[i].num_controls; c++) {
                if (windows[i].controls[c].hwnd == (uint64_t)graphics) {
                    parent_slot = i;
                    c_idx = c;
                    break;
                }
            }
        }
    }
    if (parent_slot != -1) {
        uint32_t* img_data = (uint32_t*)image;"""

new_draw = """extern "C" WIN_ABI int meinos_GdipDrawImageRectI(void* graphics, void* image, int x, int y, int width, int height) {
    int parent_slot = -1;
    int c_idx = -1;
    for (int i = 0; i < 20; i++) {
        if (windows[i].active) {
            for (int c = 0; c < windows[i].num_controls; c++) {
                if (windows[i].controls[c].hwnd == (uint64_t)graphics) {
                    parent_slot = i;
                    c_idx = c;
                    break;
                }
            }
        }
    }
    if (parent_slot != -1 && image) {
        MeinosGpImage* img = (MeinosGpImage*)image;
        // Basic scaling/drawing loop
        Control* ctrl = &windows[parent_slot].controls[c_idx];
        float scale_x = (float)img->w / width;
        float scale_y = (float)img->h / height;
        for (int dy = 0; dy < height; dy++) {
            for (int dx = 0; dx < width; dx++) {
                int sy = (int)(dy * scale_y);
                int sx = (int)(dx * scale_x);
                if (sy >= 0 && sy < img->h && sx >= 0 && sx < img->w) {
                    uint32_t pixel = img->pixels[sy * img->w + sx];
                    // Very simple alpha blending over existing buffer (assumes buffer is 0xRRGGBB)
                    uint8_t a = (pixel >> 24) & 0xFF;
                    if (a > 0) {
                        int out_x = x + dx;
                        int out_y = y + dy;
                        if (out_x >= 0 && out_x < ctrl->w && out_y >= 0 && out_y < ctrl->h) {
                            uint32_t* bb = (uint32_t*)ctrl->pixel_buffer;
                            uint32_t bg = bb[out_y * ctrl->w + out_x];
                            // Blend
                            uint8_t r = (((pixel >> 16) & 0xFF) * a + ((bg >> 16) & 0xFF) * (255 - a)) / 255;
                            uint8_t g = (((pixel >> 8) & 0xFF) * a + ((bg >> 8) & 0xFF) * (255 - a)) / 255;
                            uint8_t b = (((pixel >> 0) & 0xFF) * a + ((bg >> 0) & 0xFF) * (255 - a)) / 255;
                            bb[out_y * ctrl->w + out_x] = (r << 16) | (g << 8) | b;
                        }
                    }
                }
            }
        }
"""
content = content.replace(old_draw, new_draw)

# Clean up the rest of the old GdipDrawImageRectI (it had a loop over img_data)
# We just replace the original inner loop with nothing since we replaced it above.
content = content.replace("""        for(int dy = 0; dy < height; dy++) {
            for(int dx = 0; dx < width; dx++) {
                int out_x = x + dx;
                int out_y = y + dy;
                if (out_x >= 0 && out_x < windows[parent_slot].controls[c_idx].w && out_y >= 0 && out_y < windows[parent_slot].controls[c_idx].h) {
                    uint32_t* bb = (uint32_t*)windows[parent_slot].controls[c_idx].pixel_buffer;
                    bb[out_y * windows[parent_slot].controls[c_idx].w + out_x] = img_data[dy * width + dx];
                }
            }
        }
    }""", "    }")

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
