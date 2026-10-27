#include <stdio.h>
#include <string.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
int main() {
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "/opt/meinos/model_0.glb", &data);
    if (result == cgltf_result_success) {
        printf("Model 0 textures: %zu\n", data->textures_count);
    }
    return 0;
}
