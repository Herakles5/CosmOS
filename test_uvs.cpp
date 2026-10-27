#include <stdio.h>
#include <string.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

int main() {
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "/opt/meinos/model_10.glb", &data);
    if (result == cgltf_result_success) {
        cgltf_load_buffers(&options, data, "/opt/meinos/model_10.glb");
        cgltf_primitive* prim = &data->meshes[0].primitives[0];
        cgltf_accessor* texcoord_acc = NULL;
        for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
            if (prim->attributes[k].type == cgltf_attribute_type_texcoord && prim->attributes[k].index == 0) {
                texcoord_acc = prim->attributes[k].data;
            }
        }
        if (texcoord_acc) {
            printf("Found TEXCOORD_0 with %zu elements. First 10:\n", texcoord_acc->count);
            for (cgltf_size v = 0; v < 10 && v < texcoord_acc->count; v++) {
                float uv[2] = {0};
                cgltf_accessor_read_float(texcoord_acc, v, uv, 2);
                printf("  UV %zu: %f, %f\n", v, uv[0], uv[1]);
            }
        } else {
            printf("TEXCOORD_0 NOT FOUND!\n");
        }
    }
    return 0;
}
