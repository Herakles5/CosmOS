#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    FILE* f = fopen("Roboto.ttf", "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* ttf_buffer = (unsigned char*)malloc(size);
    fread(ttf_buffer, 1, size, f);
    fclose(f);

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0))) return 1;

    // We want a font height of roughly 18 pixels (small but readable for UI)
    float scale = stbtt_ScaleForPixelHeight(&font, 18.0f);
    
    FILE* out = fopen("cosmos_font.h", "w");
    fprintf(out, "// Auto-generated Font Header for CosmOS\n");
    fprintf(out, "#pragma once\n#include <stdint.h>\n\n");
    
    fprintf(out, "const uint8_t font_advances[96] = {");
    
    int advances[96];
    int max_h = 24;
    int max_w = 24;

    for (int i = 32; i < 128; ++i) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, i, &advance, &lsb);
        advances[i-32] = (int)(advance * scale);
        fprintf(out, "%d%s", advances[i-32], (i == 127) ? "" : ", ");
    }
    fprintf(out, "};\n\n");

    fprintf(out, "const uint8_t font_pixels[96][%d][%d] = {\n", max_h, max_w);
    
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);

    for (int i = 32; i < 128; ++i) {
        int w, h, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, i, &w, &h, &xoff, &yoff);
        
        fprintf(out, "  { // Char %d '%c'\n", i, (i == '\\' || i == '\'' || i == '"') ? ' ' : i);
        for (int y = 0; y < max_h; ++y) {
            fprintf(out, "    {");
            for (int x = 0; x < max_w; ++x) {
                int src_y = y - (baseline + yoff);
                int src_x = x - xoff;
                
                uint8_t val = 0;
                if (src_x >= 0 && src_x < w && src_y >= 0 && src_y < h) {
                    val = bitmap[src_y * w + src_x];
                }
                fprintf(out, "0x%02X%s", val, (x == max_w - 1) ? "" : ",");
            }
            fprintf(out, "}%s\n", (y == max_h - 1) ? "" : ",");
        }
        fprintf(out, "  }%s\n", (i == 127) ? "" : ",");
        
        if (bitmap) stbtt_FreeBitmap(bitmap, 0);
    }
    fprintf(out, "};\n");
    fclose(out);
    free(ttf_buffer);
    return 0;
}
