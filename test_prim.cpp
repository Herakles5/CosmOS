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
        for (cgltf_size i=0; i<data->meshes_count; i++) {
            for (cgltf_size j=0; j<data->meshes[i].primitives_count; j++) {
                cgltf_primitive* prim = &data->meshes[i].primitives[j];
                printf("Mesh %zu Prim %zu\n", i, j);
                for (cgltf_size k=0; k<prim->attributes_count; k++) {
                    printf("  Attr %zu: type %d, index %d\n", k, prim->attributes[k].type, prim->attributes[k].index);
                }
            }
        }
    }
    return 0;
}
