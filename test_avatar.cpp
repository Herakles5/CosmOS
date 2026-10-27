#include <stdio.h>
#include <string.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

int main() {
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "/root/coding/MeinOS/avatar/model_0.glb", &data);
    printf("Result: %d\n", result);
    if (result == cgltf_result_success) {
        printf("Parsed successfully! Meshes: %ld\n", data->meshes_count);
    }
    return 0;
}
