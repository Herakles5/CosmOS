#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "avatar_anim.h"
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include "stb_image.h"
#include <math.h>

#include <strings.h>

static quat quat_from_axis_angle(vec3 axis, float angle) {
    float half_angle = angle * 0.5f;
    float s = sinf(half_angle);
    return quat(axis.x * s, axis.y * s, axis.z * s, cosf(half_angle));
}

static quat quat_mul(const quat& q1, const quat& q2) {
    return quat(
        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w,
        q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
    );
}

static mat4 mat4_from_cgltf(const cgltf_float* src) {
    mat4 res;
    for (int i = 0; i < 16; ++i) res.m[i] = src[i];
    return res;
}

SkeletalAvatar::SkeletalAvatar()
    : auto_scale(1.0f), model_size(1, 1, 1), loaded(false),
      active_anim_idx(0), anim_blend_weight(1.0f),
      current_time(0), max_time(0), gltf_data(nullptr) {}

SkeletalAvatar::~SkeletalAvatar() {
    if (gltf_data) {
        cgltf_free((cgltf_data*)gltf_data);
    }
}

bool SkeletalAvatar::load_glb(const char* filepath) {
    // Clean up old model data before loading new one
    if (loaded) {
        // Delete old OpenGL textures
        for (size_t i = 0; i < materials.size(); i++) {
            if (materials[i].texture_id != 0) {
                glDeleteTextures(1, &materials[i].texture_id);
            }
        }
        // Free old gltf data
        if (gltf_data) {
            cgltf_free((cgltf_data*)gltf_data);
            gltf_data = nullptr;
        }
        // Clear all vectors
        base_vertices.clear();
        skinned_positions.clear();
        skinned_normals.clear();
        indices.clear();
        primitives.clear();
        materials.clear();
        uvs.clear();
        bones.clear();
        loaded = false;
        active_anim_idx = 0;
        anim_blend_weight = 1.0f;
        current_time = 0;
        max_time = 0;
    }
    
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filepath, &data);
    
    if (result != cgltf_result_success) return false;
    
    result = cgltf_load_buffers(&options, data, filepath);
    if (result != cgltf_result_success) {
        cgltf_free(data);
        return false;
    }
    
    gltf_data = data;
    
    // Load textures
    std::vector<GLuint> gl_textures(data->textures_count, 0);
    for (cgltf_size i = 0; i < data->textures_count; i++) {
        cgltf_texture* tex = &data->textures[i];
        if (tex->image && tex->image->buffer_view && tex->image->buffer_view->buffer->data) {
            uint8_t* ptr = (uint8_t*)tex->image->buffer_view->buffer->data + tex->image->buffer_view->offset;
            size_t size = tex->image->buffer_view->size;
            int w, h, comp;
            unsigned char* pixels = stbi_load_from_memory(ptr, size, &w, &h, &comp, 4);
            if (pixels) {
                GLuint tex_id = 0;
                glGenTextures(1, &tex_id);
                glBindTexture(GL_TEXTURE_2D, tex_id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                stbi_image_free(pixels);
                gl_textures[i] = tex_id;
                printf("Loaded texture %d: %dx%d (ID %u)\n", (int)i, w, h, tex_id);
                FILE* log = fopen("/tmp/meinos_debug.log", "a");
                if (log) { fprintf(log, "Loaded texture %d: %dx%d (ID %u)\n", (int)i, w, h, tex_id); fclose(log); }
            } else {
                printf("FAILED to load texture %d (size %zu) from memory\n", (int)i, size);
                FILE* log = fopen("/tmp/meinos_debug.log", "a");
                if (log) { fprintf(log, "FAILED to load texture %d (size %zu)\n", (int)i, size); fclose(log); }
            }
        } else {
            printf("Texture %d missing image data or buffer view\n", (int)i);
            FILE* log = fopen("/tmp/meinos_debug.log", "a");
            if (log) { fprintf(log, "Texture %d missing image data\n", (int)i); fclose(log); }
        }
    }
    
    // Load materials
    materials.resize(data->materials_count);
    for (cgltf_size i = 0; i < data->materials_count; i++) {
        cgltf_material* mat = &data->materials[i];
        materials[i].texture_id = 0;
        materials[i].base_color[0] = 1.0f;
        materials[i].base_color[1] = 1.0f;
        materials[i].base_color[2] = 1.0f;
        materials[i].base_color[3] = 1.0f;
        
        if (mat->has_pbr_metallic_roughness) {
            materials[i].base_color[0] = mat->pbr_metallic_roughness.base_color_factor[0];
            materials[i].base_color[1] = mat->pbr_metallic_roughness.base_color_factor[1];
            materials[i].base_color[2] = mat->pbr_metallic_roughness.base_color_factor[2];
            materials[i].base_color[3] = mat->pbr_metallic_roughness.base_color_factor[3];
            
            if (mat->pbr_metallic_roughness.base_color_texture.texture) {
                int tex_idx = mat->pbr_metallic_roughness.base_color_texture.texture - data->textures;
                if (tex_idx >= 0 && tex_idx < (int)data->textures_count) {
                    materials[i].texture_id = gl_textures[tex_idx];
                }
            }
        } else if (mat->has_pbr_specular_glossiness) {
            materials[i].base_color[0] = mat->pbr_specular_glossiness.diffuse_factor[0];
            materials[i].base_color[1] = mat->pbr_specular_glossiness.diffuse_factor[1];
            materials[i].base_color[2] = mat->pbr_specular_glossiness.diffuse_factor[2];
            materials[i].base_color[3] = mat->pbr_specular_glossiness.diffuse_factor[3];
            
            if (mat->pbr_specular_glossiness.diffuse_texture.texture) {
                int tex_idx = mat->pbr_specular_glossiness.diffuse_texture.texture - data->textures;
                if (tex_idx >= 0 && tex_idx < (int)data->textures_count) {
                    materials[i].texture_id = gl_textures[tex_idx];
                }
            }
        }
        materials[i].double_sided = mat->double_sided;
        materials[i].alpha_blend = (mat->alpha_mode == cgltf_alpha_mode_blend);
        materials[i].alpha_test = (mat->alpha_mode == cgltf_alpha_mode_mask);
    }
    
    // Parse bones (nodes that are joints) + compute true non-joint parent world transform
    if (data->skins_count > 0) {
        cgltf_skin* skin = &data->skins[0];
        bones.resize(skin->joints_count);
        
        for (cgltf_size i = 0; i < skin->joints_count; i++) {
            cgltf_node* node = skin->joints[i];
            bones[i].name = node->name ? node->name : "";
            
            // Find parent within skin joints
            bones[i].parent_index = -1;
            for (cgltf_size p = 0; p < skin->joints_count; p++) {
                if (skin->joints[p] == node->parent) {
                    bones[i].parent_index = (int)p;
                    break;
                }
            }

            if (bones[i].parent_index == -1 && node->parent != nullptr) {
                cgltf_float wm[16];
                cgltf_node_transform_world(node->parent, wm);
                bones[i].parent_world_matrix = mat4_from_cgltf(wm);
            } else {
                bones[i].parent_world_matrix = mat4();
            }
            
            // Inverse bind matrix
            if (skin->inverse_bind_matrices) {
                cgltf_accessor* ibm_acc = skin->inverse_bind_matrices;
                cgltf_accessor_read_float(ibm_acc, i, bones[i].inverse_bind_matrix.m, 16);
            }
            
            // Initial transform
            if (node->has_translation) {
                bones[i].local_t = vec3(node->translation[0], node->translation[1], node->translation[2]);
            } else {
                bones[i].local_t = vec3(0,0,0);
            }
            bones[i].base_local_t = bones[i].local_t;

            if (node->has_rotation) {
                bones[i].local_r = quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
            } else {
                bones[i].local_r = quat(0,0,0,1);
            }
            bones[i].base_local_r = bones[i].local_r;

            if (node->has_scale) {
                bones[i].local_s = vec3(node->scale[0], node->scale[1], node->scale[2]);
            } else {
                bones[i].local_s = vec3(1,1,1);
            }
            bones[i].base_local_s = bones[i].local_s;
        }
    }
    
    // Parse meshes via scene nodes so every non-skinned primitive inherits full glTF node world transform (Y-up)
    auto append_mesh_node = [&](cgltf_mesh* mesh, const mat4& node_world, bool node_has_skin) {
        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* prim = &mesh->primitives[j];
            
            cgltf_accessor* pos_acc = NULL;
            cgltf_accessor* norm_acc = NULL;
            cgltf_accessor* joint_acc = NULL;
            cgltf_accessor* weight_acc = NULL;
            cgltf_accessor* texcoord_acc = NULL;
            cgltf_accessor* color_acc = NULL;
            
            for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
                if (prim->attributes[k].type == cgltf_attribute_type_position) pos_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_normal) norm_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_color && prim->attributes[k].index == 0) color_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_joints && prim->attributes[k].index == 0) joint_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_weights && prim->attributes[k].index == 0) weight_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_texcoord && prim->attributes[k].index == 0) texcoord_acc = prim->attributes[k].data;
            }
            
            if (!pos_acc) continue;
            
            bool is_skinned_prim = (node_has_skin && joint_acc != NULL && weight_acc != NULL && bones.size() > 0);
            size_t vertex_offset = base_vertices.size();
            base_vertices.resize(vertex_offset + pos_acc->count);
            skinned_positions.resize(base_vertices.size());
            skinned_normals.resize(base_vertices.size());
            uvs.resize(base_vertices.size());
            colors.resize(base_vertices.size());
            
            for (cgltf_size v = 0; v < pos_acc->count; ++v) {
                AnimVertex& vert = base_vertices[vertex_offset + v];
                float pos[3] = {0};
                cgltf_accessor_read_float(pos_acc, v, pos, 3);
                vec3 raw_pos(pos[0], pos[1], pos[2]);
                
                vec3 raw_norm(0, 1, 0);
                if (norm_acc) {
                    float norm[3] = {0};
                    cgltf_accessor_read_float(norm_acc, v, norm, 3);
                    raw_norm = vec3(norm[0], norm[1], norm[2]);
                }

                if (is_skinned_prim) {
                    vert.position = raw_pos;
                    vert.normal = raw_norm;
                } else {
                    vert.position = mat4_mul_vec3(node_world, raw_pos);
                    vert.normal = vec3_normalize(mat4_mul_dir(node_world, raw_norm));
                }
                
                if (texcoord_acc) {
                    float uv[2] = {0};
                    cgltf_accessor_read_float(texcoord_acc, v, uv, 2);
                    vert.uv = vec2(uv[0], uv[1]);
                    uvs[vertex_offset + v] = vert.uv;
                } else {
                    vert.uv = vec2(0,0);
                    uvs[vertex_offset + v] = vec2(0,0);
                }
                
                if (color_acc) {
                    float col[4] = {1,1,1,1};
                    cgltf_accessor_read_float(color_acc, v, col, 4);
                    vert.color = vec3(col[0], col[1], col[2]);
                    colors[vertex_offset + v] = vert.color;
                } else {
                    vert.color = vec3(1,1,1);
                    colors[vertex_offset + v] = vec3(1,1,1);
                }
                
                if (is_skinned_prim) {
                    cgltf_uint jts[4] = {0};
                    float wts[4] = {0};
                    cgltf_accessor_read_uint(joint_acc, v, jts, 4);
                    cgltf_accessor_read_float(weight_acc, v, wts, 4);
                    float wsum = 0.0f;
                    for (int n=0; n<4; n++) {
                        vert.joints[n] = (jts[n] < bones.size()) ? (uint16_t)jts[n] : 0;
                        vert.weights[n] = wts[n];
                        wsum += wts[n];
                    }
                    if (wsum < 1e-5f) {
                        // Safeguard unweighted vertex in skinned primitive
                        vert.joints[0] = 0;
                        vert.weights[0] = 1.0f;
                    }
                } else {
                    for (int n=0; n<4; n++) {
                        vert.joints[n] = 0;
                        vert.weights[n] = 0.0f;
                    }
                }
            }
            
            // Indices
            Primitive out_prim;
            out_prim.index_offset = indices.size();
            out_prim.material_idx = -1;
            
            if (prim->material) {
                out_prim.material_idx = (int)(prim->material - data->materials);
            }
            
            if (prim->indices) {
                out_prim.index_count = prim->indices->count;
                indices.resize(out_prim.index_offset + out_prim.index_count);
                for (cgltf_size id = 0; id < prim->indices->count; ++id) {
                    indices[out_prim.index_offset + id] = (uint32_t)(cgltf_accessor_read_index(prim->indices, id) + vertex_offset);
                }
            } else {
                out_prim.index_count = pos_acc->count;
                indices.resize(out_prim.index_offset + out_prim.index_count);
                for (cgltf_size id = 0; id < pos_acc->count; ++id) {
                    indices[out_prim.index_offset + id] = (uint32_t)(vertex_offset + id);
                }
            }
            
            primitives.push_back(out_prim);
        }
    };

    bool parsed_any_node_mesh = false;
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        if (data->nodes[n].mesh) {
            cgltf_float wm[16];
            cgltf_node_transform_world(&data->nodes[n], wm);
            append_mesh_node(data->nodes[n].mesh, mat4_from_cgltf(wm), data->nodes[n].skin != nullptr);
            parsed_any_node_mesh = true;
        }
    }
    if (!parsed_any_node_mesh) {
        mat4 ident;
        for (cgltf_size i = 0; i < data->meshes_count; ++i) {
            append_mesh_node(&data->meshes[i], ident, data->skins_count > 0);
        }
    }
    
    // Preserve legacy root_transform field for inspection using full world matrix of scene root
    root_transform = mat4();
    if (data->scenes_count > 0 && data->scenes[0].nodes_count > 0) {
        cgltf_float wm[16];
        cgltf_node_transform_world(data->scenes[0].nodes[0], wm);
        root_transform = mat4_from_cgltf(wm);
    }

    // Initialize default clip (0) and evaluate upright t=0 pose in true glTF Y-up world space
    loaded = true;
    active_anim_idx = 0;
    anim_blend_weight = 1.0f;
    current_time = 0.0f;
    set_animation_clip(0, true);
    update(0.0f);
    skin();
    
    // Calculate bounding box for auto-scaling directly in upright glTF +Y up world space
    if (skinned_positions.size() > 0) {
        vec3 min_p = skinned_positions[0];
        vec3 max_p = skinned_positions[0];
        for (size_t i = 1; i < skinned_positions.size(); i++) {
            const vec3& p = skinned_positions[i];
            if (p.x < min_p.x) min_p.x = p.x;
            if (p.y < min_p.y) min_p.y = p.y;
            if (p.z < min_p.z) min_p.z = p.z;
            if (p.x > max_p.x) max_p.x = p.x;
            if (p.y > max_p.y) max_p.y = p.y;
            if (p.z > max_p.z) max_p.z = p.z;
        }
        center_offset = vec3(
            -(min_p.x + max_p.x) * 0.5f,
            -min_p.y, // Align feet/paws cleanly to Y=0 floor plane
            -(min_p.z + max_p.z) * 0.5f
        );
        float size_x = max_p.x - min_p.x;
        float size_y = max_p.y - min_p.y;
        float size_z = max_p.z - min_p.z;
        model_size = vec3(size_x, size_y, size_z);
        float max_size = size_y;
        if (size_x > max_size) max_size = size_x;
        if (size_z > max_size) max_size = size_z;
        
        if (max_size > 0.001f) {
            auto_scale = 1.5f / max_size;
        } else {
            auto_scale = 1.0f;
        }
    } else {
        auto_scale = 1.0f;
        center_offset = vec3(0,0,0);
        model_size = vec3(1,1,1);
    }
    
    return true;
}

void SkeletalAvatar::update_hierarchy(int bone_idx, const mat4& parent_mat) {
    if (bone_idx < 0 || bone_idx >= (int)bones.size()) return;
    
    Bone& b = bones[bone_idx];
    mat4 local = mat4_from_trs(b.local_t, b.local_r, b.local_s);
    b.global_matrix = mat4_mul(parent_mat, local);
    
    for (int i = 0; i < (int)bones.size(); ++i) {
        if (bones[i].parent_index == bone_idx) {
            update_hierarchy(i, b.global_matrix);
        }
    }
}

int SkeletalAvatar::get_animation_count() const {
    if (!loaded || !gltf_data) return 0;
    return (int)((cgltf_data*)gltf_data)->animations_count;
}

const char* SkeletalAvatar::get_animation_name(int idx) const {
    if (!loaded || !gltf_data) return "";
    cgltf_data* data = (cgltf_data*)gltf_data;
    if (idx < 0 || idx >= (int)data->animations_count) return "";
    return data->animations[idx].name ? data->animations[idx].name : "";
}

void SkeletalAvatar::set_animation_clip(int idx, bool reset_time) {
    if (!loaded || !gltf_data) return;
    cgltf_data* data = (cgltf_data*)gltf_data;
    if (data->animations_count == 0) {
        active_anim_idx = 0;
        max_time = 0.0f;
        current_time = 0.0f;
        return;
    }
    if (idx < 0) idx = 0;
    if (idx >= (int)data->animations_count) idx = (int)data->animations_count - 1;
    bool changed = (active_anim_idx != idx);
    active_anim_idx = idx;

    max_time = 0.0f;
    cgltf_animation* anim = &data->animations[active_anim_idx];
    for (cgltf_size i = 0; i < anim->samplers_count; ++i) {
        cgltf_accessor* input = anim->samplers[i].input;
        if (input && input->max[0] > max_time) {
            max_time = input->max[0];
        }
    }
    if (reset_time || changed || (max_time > 0.0f && current_time >= max_time)) {
        current_time = 0.0f;
    }
}

int SkeletalAvatar::find_animation_clip(const char* keyword) const {
    if (!loaded || !gltf_data || !keyword) return -1;
    cgltf_data* data = (cgltf_data*)gltf_data;
    for (int i = 0; i < (int)data->animations_count; ++i) {
        const char* nm = data->animations[i].name;
        if (nm && strcasestr(nm, keyword) != nullptr) {
            return i;
        }
    }
    return -1;
}

void SkeletalAvatar::set_current_time(float t) {
    if (max_time <= 0.0f) {
        current_time = 0.0f;
        return;
    }
    while (t < 0.0f) t += max_time;
    while (t >= max_time) t -= max_time;
    current_time = t;
}

void SkeletalAvatar::reset_to_bind_pose() {
    for (size_t i = 0; i < bones.size(); ++i) {
        bones[i].local_t = bones[i].base_local_t;
        bones[i].local_r = bones[i].base_local_r;
        bones[i].local_s = bones[i].base_local_s;
    }
}

void SkeletalAvatar::update(float delta_time) {
    if (!loaded) return;
    
    // Reset all bones to their base local transform
    reset_to_bind_pose();
    
    cgltf_data* data = (cgltf_data*)gltf_data;
    if (max_time > 0.0f && data && active_anim_idx >= 0 && active_anim_idx < (int)data->animations_count) {
        current_time += delta_time;
        while (current_time >= max_time) current_time -= max_time;
        while (current_time < 0.0f) current_time += max_time;

        if (anim_blend_weight > 0.001f) {
            cgltf_animation* anim = &data->animations[active_anim_idx];
            
            for (cgltf_size i = 0; i < anim->channels_count; ++i) {
                cgltf_animation_channel* channel = &anim->channels[i];
                cgltf_node* node = channel->target_node;
                if (!node || data->skins_count == 0) continue;
                
                // Find bone index for this node
                int b_idx = -1;
                for (size_t b = 0; b < bones.size(); b++) {
                    if (data->skins[0].joints[b] == node) {
                        b_idx = (int)b;
                        break;
                    }
                }
                
                if (b_idx == -1) continue;

                cgltf_animation_sampler* sampler = channel->sampler;
                if (!sampler || !sampler->input || !sampler->output) continue;
                cgltf_accessor* input = sampler->input;
                cgltf_accessor* output = sampler->output;
                if (input->count == 0) continue;

                cgltf_size k0 = 0, k1 = 0;
                float factor = 0.0f;

                if (input->count == 1) {
                    k0 = k1 = 0;
                } else {
                    float t_first = 0.0f, t_last = 0.0f;
                    cgltf_accessor_read_float(input, 0, &t_first, 1);
                    cgltf_accessor_read_float(input, input->count - 1, &t_last, 1);

                    if (current_time <= t_first) {
                        k0 = k1 = 0;
                    } else if (current_time >= t_last) {
                        k0 = k1 = input->count - 1;
                    } else {
                        for (cgltf_size k = 0; k < input->count - 1; ++k) {
                            float t0 = 0.0f, t1 = 0.0f;
                            cgltf_accessor_read_float(input, k, &t0, 1);
                            cgltf_accessor_read_float(input, k + 1, &t1, 1);
                            if (current_time >= t0 && current_time <= t1) {
                                k0 = k;
                                k1 = k + 1;
                                float span = t1 - t0;
                                factor = (span > 1e-6f) ? ((current_time - t0) / span) : 0.0f;
                                break;
                            }
                        }
                    }
                }

                if (channel->target_path == cgltf_animation_path_type_translation) {
                    float v0[3] = {0}, v1[3] = {0};
                    cgltf_accessor_read_float(output, k0, v0, 3);
                    cgltf_accessor_read_float(output, k1, v1, 3);
                    vec3 anim_v = vec3_lerp(vec3(v0[0], v0[1], v0[2]), vec3(v1[0], v1[1], v1[2]), factor);
                    bones[b_idx].local_t = (anim_blend_weight >= 0.999f) ? anim_v : vec3_lerp(bones[b_idx].base_local_t, anim_v, anim_blend_weight);
                } else if (channel->target_path == cgltf_animation_path_type_rotation) {
                    float q0[4] = {0,0,0,1}, q1[4] = {0,0,0,1};
                    cgltf_accessor_read_float(output, k0, q0, 4);
                    cgltf_accessor_read_float(output, k1, q1, 4);
                    quat anim_q = quat_slerp(quat(q0[0], q0[1], q0[2], q0[3]), quat(q1[0], q1[1], q1[2], q1[3]), factor);
                    bones[b_idx].local_r = (anim_blend_weight >= 0.999f) ? anim_q : quat_slerp(bones[b_idx].base_local_r, anim_q, anim_blend_weight);
                } else if (channel->target_path == cgltf_animation_path_type_scale) {
                    float s0[3] = {1,1,1}, s1[3] = {1,1,1};
                    cgltf_accessor_read_float(output, k0, s0, 3);
                    cgltf_accessor_read_float(output, k1, s1, 3);
                    vec3 anim_s = vec3_lerp(vec3(s0[0], s0[1], s0[2]), vec3(s1[0], s1[1], s1[2]), factor);
                    bones[b_idx].local_s = (anim_blend_weight >= 0.999f) ? anim_s : vec3_lerp(bones[b_idx].base_local_s, anim_s, anim_blend_weight);
                }
            }
        }
    }
    
    // Apply procedural animations (waving, peace sign)
    apply_procedural_animations(delta_time);
    
    // Update global matrices using each root joint's true glTF parent world transform
    recompute_global_matrices();
}

void SkeletalAvatar::skin() {
    if (!loaded) return;
    
    std::vector<mat4> joint_matrices(bones.size());
    for (size_t i = 0; i < bones.size(); ++i) {
        joint_matrices[i] = mat4_mul(bones[i].global_matrix, bones[i].inverse_bind_matrix);
    }
    
    for (size_t i = 0; i < base_vertices.size(); ++i) {
        AnimVertex& v = base_vertices[i];
        vec3 final_pos(0,0,0);
        vec3 final_norm(0,0,0);
        
        if (bones.size() > 0) {
            bool has_weight = false;
            for (int j = 0; j < 4; ++j) {
                if (v.weights[j] > 0.0f && v.joints[j] < bones.size()) {
                    mat4& joint_mat = joint_matrices[v.joints[j]];
                    final_pos = final_pos + (mat4_mul_vec3(joint_mat, v.position) * v.weights[j]);
                    final_norm = final_norm + (mat4_mul_dir(joint_mat, v.normal) * v.weights[j]);
                    has_weight = true;
                }
            }
            if (has_weight) {
                skinned_positions[i] = final_pos;
                skinned_normals[i] = vec3_normalize(final_norm);
                continue;
            }
        }
        // Unskinned / static vertices are already transformed into glTF Y-up world space at load time
        skinned_positions[i] = v.position;
        skinned_normals[i] = v.normal;
    }
}

int SkeletalAvatar::find_bone(const char* name) {
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].name.find(name) != std::string::npos) {
            return i;
        }
    }
    return -1;
}

Bone* SkeletalAvatar::get_bone(int index) {
    if (index >= 0 && index < (int)bones.size()) {
        return &bones[index];
    }
    return nullptr;
}

void SkeletalAvatar::draw() {
    if (!loaded) return;
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    
    glVertexPointer(3, GL_FLOAT, sizeof(vec3), skinned_positions.data());
    glTexCoordPointer(2, GL_FLOAT, sizeof(vec2), uvs.data());
    glColorPointer(3, GL_FLOAT, sizeof(vec3), colors.data());
    if (skinned_normals.size() > 0) {
        glNormalPointer(GL_FLOAT, sizeof(vec3), skinned_normals.data());
    }
    
    for (size_t i = 0; i < primitives.size(); i++) {
        Primitive& prim = primitives[i];
        
        if (prim.material_idx >= 0 && prim.material_idx < (int)materials.size()) {
            Material& mat = materials[prim.material_idx];
            
            if (mat.texture_id != 0) {
                // Textured AND lit
                glEnable(GL_TEXTURE_2D);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glBindTexture(GL_TEXTURE_2D, mat.texture_id);
                
                glEnable(GL_LIGHTING);
                glEnable(GL_LIGHT0);
                float light_pos[] = {0.5f, 1.0f, 1.0f, 0.0f};
                float light_amb[] = {0.3f, 0.3f, 0.35f, 1.0f};
                float light_dif[] = {0.8f, 0.75f, 0.7f, 1.0f};
                glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
                glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);
                glLightfv(GL_LIGHT0, GL_DIFFUSE, light_dif);
                
                float mat_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_color);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glEnable(GL_COLOR_MATERIAL);
                glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
            } else {
                // Untextured: enable lighting for proper 3D shading
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                glEnable(GL_LIGHTING);
                glEnable(GL_LIGHT0);
                // Set up a directional light from upper-front-right
                float light_pos[] = {0.5f, 1.0f, 1.0f, 0.0f};
                float light_amb[] = {0.3f, 0.3f, 0.35f, 1.0f};
                float light_dif[] = {0.8f, 0.75f, 0.7f, 1.0f};
                glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
                glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);
                glLightfv(GL_LIGHT0, GL_DIFFUSE, light_dif);
                // Material color from base_color
                float mat_color[] = {mat.base_color[0], mat.base_color[1], mat.base_color[2], mat.base_color[3]};
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_color);
                glColor4fv(mat.base_color);
                glEnable(GL_COLOR_MATERIAL);
                glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            
            if (mat.double_sided) {
                glDisable(GL_CULL_FACE);
                glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
            } else {
                glEnable(GL_CULL_FACE);
                glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
            }
        } else {
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_LIGHTING);
            glColor4f(0.8f, 0.8f, 0.8f, 1.0f);
            glBindTexture(GL_TEXTURE_2D, 0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
        }
        
        glDrawElements(GL_TRIANGLES, prim.index_count, GL_UNSIGNED_INT, &indices[prim.index_offset]);
    }
    
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SkeletalAvatar::apply_procedural_animations(float delta_time) {
    if (is_waving_left || is_waving_right) {
        wave_time += delta_time * 6.0f;
    } else {
        if (wave_time > 0) {
            wave_time -= delta_time * 2.0f;
            if (wave_time < 0) wave_time = 0;
        }
    }
    
    if (is_peace_sign) {
        peace_sign_weight += delta_time * 4.0f;
        if (peace_sign_weight > 1.0f) peace_sign_weight = 1.0f;
    } else {
        peace_sign_weight -= delta_time * 4.0f;
        if (peace_sign_weight < 0.0f) peace_sign_weight = 0.0f;
    }
    
    float wave_blend = wave_time > 1.0f ? 1.0f : wave_time;

    // Wave Left
    if (is_waving_left || wave_blend > 0) {
        int b_arm = find_bone("arm_stretch.l_041");
        int b_forearm = find_bone("forearm_stretch.l_042");
        if (b_arm != -1 && b_forearm != -1) {
            quat q_arm = quat_from_axis_angle(vec3(0,0,1), 1.2f); 
            quat q_forearm = quat_from_axis_angle(vec3(1,0,0), sinf(wave_time) * 0.6f);
            bones[b_arm].local_r = quat_slerp(bones[b_arm].local_r, quat_mul(bones[b_arm].local_r, q_arm), wave_blend * (is_waving_left ? 1.0f : 0.0f));
            bones[b_forearm].local_r = quat_slerp(bones[b_forearm].local_r, quat_mul(bones[b_forearm].local_r, q_forearm), wave_blend * (is_waving_left ? 1.0f : 0.0f));
        }
    }

    // Wave Right
    if (is_waving_right || wave_blend > 0) {
        int b_arm = find_bone("arm_stretch.r_020");
        int b_forearm = find_bone("forearm_stretch.r_021");
        if (b_arm != -1 && b_forearm != -1) {
            quat q_arm = quat_from_axis_angle(vec3(0,0,-1), 1.2f); 
            quat q_forearm = quat_from_axis_angle(vec3(1,0,0), sinf(wave_time) * 0.6f);
            bones[b_arm].local_r = quat_slerp(bones[b_arm].local_r, quat_mul(bones[b_arm].local_r, q_arm), wave_blend * (is_waving_right ? 1.0f : 0.0f));
            bones[b_forearm].local_r = quat_slerp(bones[b_forearm].local_r, quat_mul(bones[b_forearm].local_r, q_forearm), wave_blend * (is_waving_right ? 1.0f : 0.0f));
        }
    }
    
    // Peace sign (curl ring, pinky, thumb)
    if (peace_sign_weight > 0) {
        const char* curl_bones[] = {
            "c_pinky1.r_026", "c_pinky2.r_027", "c_pinky3.r_028",
            "c_ring1.r_029", "c_ring2.r_030", "c_ring3.r_031",
            "c_thumb1.r_023", "c_thumb2.r_024", "c_thumb3.r_025",
            "c_pinky1.l_047", "c_pinky2.l_048", "c_pinky3.l_049",
            "c_ring1.l_050", "c_ring2.l_051", "c_ring3.l_052",
            "c_thumb1.l_044", "c_thumb2.l_045", "c_thumb3.l_046"
        };
        for (int i=0; i<18; i++) {
            int b = find_bone(curl_bones[i]);
            if (b != -1) {
                quat q_curl = quat_from_axis_angle(vec3(0,0,1), 1.2f); 
                bones[b].local_r = quat_slerp(bones[b].local_r, quat_mul(bones[b].local_r, q_curl), peace_sign_weight);
            }
        }
    }
    // End of apply_procedural_animations
}

void SkeletalAvatar::add_procedural_rotation(int bone_idx, float ax, float ay, float az, float angle) {
    if (bone_idx >= 0 && bone_idx < (int)bones.size()) {
        quat rot = quat_from_axis_angle(vec3(ax, ay, az), angle);
        bones[bone_idx].local_r = quat_mul(bones[bone_idx].local_r, rot);
    }
}

void SkeletalAvatar::recompute_global_matrices() {
    for (int i = 0; i < (int)bones.size(); ++i) {
        if (bones[i].parent_index == -1) {
            update_hierarchy(i, bones[i].parent_world_matrix);
        }
    }
}
