#include <stdio.h>
#include <stdint.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

int main() {
    cgltf_options options = {};
    cgltf_data* data = NULL;
    
    const char* files[] = {"/opt/meinos/model_7.glb", "/opt/meinos/model_10.glb"};
    for (int f = 0; f < 2; f++) {
        printf("--- %s ---\n", files[f]);
        if (cgltf_parse_file(&options, files[f], &data) != cgltf_result_success) {
            printf("Failed to parse\n");
            continue;
        }
        if (cgltf_load_buffers(&options, data, files[f]) != cgltf_result_success) {
            printf("Failed to load buffers\n");
            continue;
        }
        
        printf("Textures count: %d\n", (int)data->textures_count);
        for (cgltf_size i = 0; i < data->textures_count; i++) {
            cgltf_texture* tex = &data->textures[i];
            printf("Texture %d:\n", (int)i);
            if (tex->image) {
                printf("  image: YES\n");
                printf("  uri: %s\n", tex->image->uri ? tex->image->uri : "NULL");
                printf("  buffer_view: %p\n", tex->image->buffer_view);
                if (tex->image->buffer_view) {
                    printf("  buffer_view->buffer: %p\n", tex->image->buffer_view->buffer);
                    if (tex->image->buffer_view->buffer) {
                        printf("  buffer_view->buffer->data: %p\n", tex->image->buffer_view->buffer->data);
                    }
                }
            } else {
                printf("  image: NULL\n");
            }
        }
        cgltf_free(data);
    }
    return 0;
}
