#include <stdio.h>
#include <stdint.h>
#include <vector>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main() {
    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "/opt/meinos/model_10.glb", &data);
    if (result != cgltf_result_success) return 1;
    result = cgltf_load_buffers(&options, data, "/opt/meinos/model_10.glb");
    if (result != cgltf_result_success) return 1;

    cgltf_texture* tex = &data->textures[0];
    if (tex->image && tex->image->buffer_view && tex->image->buffer_view->buffer->data) {
        uint8_t* ptr = (uint8_t*)tex->image->buffer_view->buffer->data + tex->image->buffer_view->offset;
        size_t size = tex->image->buffer_view->size;
        
        int w, h, comp;
        unsigned char* pixels = stbi_load_from_memory(ptr, size, &w, &h, &comp, 4);
        if (pixels) {
            printf("Loaded %dx%d. First 5 pixels:\n", w, h);
            for(int i=0; i<5; i++) {
                printf("Pixel %d: %d %d %d %d\n", i, pixels[i*4], pixels[i*4+1], pixels[i*4+2], pixels[i*4+3]);
            }
            
            // Check if ALL pixels are white
            bool all_white = true;
            for(int i=0; i<w*h; i++) {
                if (pixels[i*4] != 255 || pixels[i*4+1] != 255 || pixels[i*4+2] != 255) {
                    all_white = false;
                    break;
                }
            }
            if (all_white) printf("WARNING: ENTIRE TEXTURE IS WHITE!\n");
            else printf("Texture is NOT entirely white.\n");
            
            stbi_image_free(pixels);
        }
    }
    cgltf_free(data);
    return 0;
}
