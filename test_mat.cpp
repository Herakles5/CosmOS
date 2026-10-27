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
        printf("Materials: %zu\n", data->materials_count);
        for(cgltf_size i=0; i<data->materials_count; i++) {
            cgltf_material* mat = &data->materials[i];
            printf("Material %zu: has_pbr=%d\n", i, mat->has_pbr_metallic_roughness);
            if (mat->has_pbr_metallic_roughness) {
                printf("  base_color_texture.texture = %p\n", mat->pbr_metallic_roughness.base_color_texture.texture);
                if (mat->pbr_metallic_roughness.base_color_texture.texture) {
                    int tex_idx = mat->pbr_metallic_roughness.base_color_texture.texture - data->textures;
                    printf("  tex_idx = %d\n", tex_idx);
                }
            }
        }
    }
    return 0;
}
