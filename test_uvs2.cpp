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
        
        for (int tex_idx = 0; tex_idx < 6; tex_idx++) {
            cgltf_accessor* texcoord_acc = NULL;
            for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
                if (prim->attributes[k].type == cgltf_attribute_type_texcoord && prim->attributes[k].index == tex_idx) {
                    texcoord_acc = prim->attributes[k].data;
                }
            }
            if (texcoord_acc) {
                float min_u = 1e9, max_u = -1e9;
                float min_v = 1e9, max_v = -1e9;
                for (cgltf_size v = 0; v < texcoord_acc->count; v++) {
                    float uv[2] = {0};
                    cgltf_accessor_read_float(texcoord_acc, v, uv, 2);
                    if (uv[0] < min_u) min_u = uv[0];
                    if (uv[0] > max_u) max_u = uv[0];
                    if (uv[1] < min_v) min_v = uv[1];
                    if (uv[1] > max_v) max_v = uv[1];
                }
                printf("TEXCOORD_%d bounds: U=[%f, %f], V=[%f, %f]\n", tex_idx, min_u, max_u, min_v, max_v);
            }
        }
    }
    return 0;
}
