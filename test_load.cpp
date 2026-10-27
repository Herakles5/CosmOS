#include <stdio.h>
#include <string.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
int main() {
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "/opt/meinos/model_10.glb", &data);
    printf("Parse Result: %d\n", result);
    if (result == cgltf_result_success) {
        cgltf_result res2 = cgltf_load_buffers(&options, data, "/opt/meinos/model_10.glb");
        printf("Load Result: %d\n", res2);
        if (data->buffers_count > 0) {
            printf("Buffer 0 data: %p, size: %zu\n", data->buffers[0].data, data->buffers[0].size);
        }
        for (cgltf_size i=0; i<data->textures_count; i++) {
            cgltf_texture* tex = &data->textures[i];
            if (tex->image && tex->image->buffer_view && tex->image->buffer_view->buffer->data) {
                printf("Texture %zu has image data! Offset %zu size %zu\n", i, tex->image->buffer_view->offset, tex->image->buffer_view->size);
            } else {
                printf("Texture %zu MISSING image data! image=%p\n", i, tex->image);
                if (tex->image) printf("  buffer_view=%p uri=%s\n", tex->image->buffer_view, tex->image->uri ? tex->image->uri : "null");
            }
        }
    }
    return 0;
}
