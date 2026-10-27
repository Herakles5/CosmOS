#include <stdio.h>
#include <stdint.h>
#include <vector>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

int main() {
    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "/opt/meinos/model_10.glb", &data);
    if (result != cgltf_result_success) return 1;
    
    printf("Textures count: %d\n", (int)data->textures_count);
    for (cgltf_size i = 0; i < data->materials_count; i++) {
        cgltf_material* mat = &data->materials[i];
        printf("Material %d: has_pbr=%d\n", (int)i, mat->has_pbr_metallic_roughness);
        if (mat->has_pbr_metallic_roughness) {
            printf("  baseColorFactor: %f %f %f %f\n", 
                mat->pbr_metallic_roughness.base_color_factor[0],
                mat->pbr_metallic_roughness.base_color_factor[1],
                mat->pbr_metallic_roughness.base_color_factor[2],
                mat->pbr_metallic_roughness.base_color_factor[3]);
            if (mat->pbr_metallic_roughness.base_color_texture.texture) {
                int tex_idx = mat->pbr_metallic_roughness.base_color_texture.texture - data->textures;
                printf("  texture_idx: %d\n", tex_idx);
            } else {
                printf("  no texture!\n");
            }
        }
    }
    
    cgltf_free(data);
    return 0;
}
