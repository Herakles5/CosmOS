#include <stdio.h>
#include <string.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
int main() {
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "/opt/meinos/model_10.glb", &data);
    if (result == cgltf_result_success) {
        cgltf_load_buffers(&options, data, "/opt/meinos/model_10.glb");
        for (cgltf_size i=0; i<data->textures_count; i++) {
            cgltf_texture* tex = &data->textures[i];
            if (tex->image && tex->image->buffer_view && tex->image->buffer_view->buffer->data) {
                uint8_t* ptr = (uint8_t*)tex->image->buffer_view->buffer->data + tex->image->buffer_view->offset;
                size_t size = tex->image->buffer_view->size;
                int w, h, comp;
                unsigned char* pixels = stbi_load_from_memory(ptr, size, &w, &h, &comp, 4);
                if (pixels) {
                    printf("Texture %zu loaded! %dx%d\n", i, w, h);
                    stbi_image_free(pixels);
                } else {
                    printf("Texture %zu FAILED to load! Reason: %s\n", i, stbi_failure_reason());
                }
            }
        }
    }
    return 0;
}
