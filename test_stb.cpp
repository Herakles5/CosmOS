#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdlib.h>

int main() {
    FILE* f = fopen("/tmp/img_0.dat", "rb");
    if (!f) { printf("No file\n"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* buf = (unsigned char*)malloc(size);
    fread(buf, 1, size, f);
    fclose(f);
    
    int w, h, comp;
    unsigned char* pixels = stbi_load_from_memory(buf, size, &w, &h, &comp, 4);
    if (!pixels) {
        printf("FAILED: %s\n", stbi_failure_reason());
    } else {
        printf("SUCCESS: %dx%d\n", w, h);
    }
    return 0;
}
