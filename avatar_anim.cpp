#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "avatar_anim.h"
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include "stb_image.h"

SkeletalAvatar::SkeletalAvatar() : loaded(false), current_time(0), max_time(0), gltf_data(nullptr) {}

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
        indices.clear();
        primitives.clear();
        materials.clear();
        uvs.clear();
        bones.clear();
        loaded = false;
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
            }
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
        }
        materials[i].double_sided = mat->double_sided;
        materials[i].alpha_blend = (mat->alpha_mode == cgltf_alpha_mode_blend);
        materials[i].alpha_test = (mat->alpha_mode == cgltf_alpha_mode_mask);
    }
    
    // Parse bones (nodes that are joints)
    if (data->skins_count > 0) {
        cgltf_skin* skin = &data->skins[0];
        bones.resize(skin->joints_count);
        
        for (cgltf_size i = 0; i < skin->joints_count; i++) {
            cgltf_node* node = skin->joints[i];
            bones[i].name = node->name ? node->name : "";
            
            // Find parent
            bones[i].parent_index = -1;
            for (cgltf_size p = 0; p < skin->joints_count; p++) {
                if (skin->joints[p] == node->parent) {
                    bones[i].parent_index = p;
                    break;
                }
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
            if (node->has_rotation) {
                bones[i].local_r = quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
            } else {
                bones[i].local_r = quat(0,0,0,1);
            }
            if (node->has_scale) {
                bones[i].local_s = vec3(node->scale[0], node->scale[1], node->scale[2]);
            } else {
                bones[i].local_s = vec3(1,1,1);
            }
        }
    }
    
    // Parse meshes
    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        cgltf_mesh* mesh = &data->meshes[i];
        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* prim = &mesh->primitives[j];
            
            cgltf_accessor* pos_acc = NULL;
            cgltf_accessor* norm_acc = NULL;
            cgltf_accessor* joint_acc = NULL;
            cgltf_accessor* weight_acc = NULL;
            
            cgltf_accessor* texcoord_acc = NULL;
            
            for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
                if (prim->attributes[k].type == cgltf_attribute_type_position) pos_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_normal) norm_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_joints) joint_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_weights) weight_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_texcoord) texcoord_acc = prim->attributes[k].data;
            }
            
            if (!pos_acc) continue;
            
            size_t vertex_offset = base_vertices.size();
            base_vertices.resize(vertex_offset + pos_acc->count);
            skinned_positions.resize(base_vertices.size());
            uvs.resize(base_vertices.size());
            
            for (cgltf_size v = 0; v < pos_acc->count; ++v) {
                AnimVertex& vert = base_vertices[vertex_offset + v];
                float pos[3] = {0};
                cgltf_accessor_read_float(pos_acc, v, pos, 3);
                vert.position = vec3(pos[0], pos[1], pos[2]);
                
                if (norm_acc) {
                    float norm[3] = {0};
                    cgltf_accessor_read_float(norm_acc, v, norm, 3);
                    vert.normal = vec3(norm[0], norm[1], norm[2]);
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
                
                if (joint_acc) {
                    cgltf_uint jts[4] = {0};
                    cgltf_accessor_read_uint(joint_acc, v, jts, 4);
                    for (int n=0; n<4; n++) vert.joints[n] = jts[n];
                } else {
                    for (int n=0; n<4; n++) vert.joints[n] = 0;
                }
                
                if (weight_acc) {
                    float wts[4] = {0};
                    cgltf_accessor_read_float(weight_acc, v, wts, 4);
                    for (int n=0; n<4; n++) vert.weights[n] = wts[n];
                } else {
                    vert.weights[0] = 1.0f;
                    for (int n=1; n<4; n++) vert.weights[n] = 0.0f;
                }
            }
            
            // Indices
            Primitive out_prim;
            out_prim.index_offset = indices.size();
            out_prim.material_idx = -1;
            
            if (prim->material) {
                out_prim.material_idx = prim->material - data->materials;
            }
            
            if (prim->indices) {
                out_prim.index_count = prim->indices->count;
                indices.resize(out_prim.index_offset + out_prim.index_count);
                for (cgltf_size id = 0; id < prim->indices->count; ++id) {
                    indices[out_prim.index_offset + id] = cgltf_accessor_read_index(prim->indices, id) + vertex_offset;
                }
            } else {
                out_prim.index_count = pos_acc->count;
                indices.resize(out_prim.index_offset + out_prim.index_count);
                for (cgltf_size id = 0; id < pos_acc->count; ++id) {
                    indices[out_prim.index_offset + id] = vertex_offset + id;
                }
            }
            
            primitives.push_back(out_prim);
        }
    }
    
    // Extract root transform
    root_transform = mat4();
    if (data->scenes_count > 0 && data->scenes[0].nodes_count > 0) {
        cgltf_node* root_node = data->scenes[0].nodes[0];
        vec3 t(0,0,0);
        quat r(0,0,0,1);
        vec3 s(1,1,1);
        if (root_node->has_translation) t = vec3(root_node->translation[0], root_node->translation[1], root_node->translation[2]);
        if (root_node->has_rotation) r = quat(root_node->rotation[0], root_node->rotation[1], root_node->rotation[2], root_node->rotation[3]);
        if (root_node->has_scale) s = vec3(root_node->scale[0], root_node->scale[1], root_node->scale[2]);
        root_transform = mat4_from_trs(t, r, s);
    }
    
    // Calculate bounding box for auto-scaling based on transformed vertices
    if (base_vertices.size() > 0) {
        vec3 first_p = mat4_mul_vec3(root_transform, base_vertices[0].position);
        vec3 min_p = first_p;
        vec3 max_p = first_p;
        for (size_t i = 1; i < base_vertices.size(); i++) {
            vec3 p = mat4_mul_vec3(root_transform, base_vertices[i].position);
            if (p.x < min_p.x) min_p.x = p.x;
            if (p.y < min_p.y) min_p.y = p.y;
            if (p.z < min_p.z) min_p.z = p.z;
            if (p.x > max_p.x) max_p.x = p.x;
            if (p.y > max_p.y) max_p.y = p.y;
            if (p.z > max_p.z) max_p.z = p.z;
        }
        center_offset = vec3(
            -(min_p.x + max_p.x) * 0.5f,
            -min_p.y, // Align to bottom
            -(min_p.z + max_p.z) * 0.5f
        );
        float size_y = max_p.y - min_p.y;
        float size_x = max_p.x - min_p.x;
        float size_z = max_p.z - min_p.z;
        float max_size = size_y;
        if (size_x > max_size) max_size = size_x;
        if (size_z > max_size) max_size = size_z;
        
        if (max_size > 0.001f) {
            auto_scale = 1.5f / max_size; // Fit within 1.5 units
        } else {
            auto_scale = 1.0f;
        }
    } else {
        auto_scale = 1.0f;
        center_offset = vec3(0,0,0);
    }
    
    // Find max time of first animation
    if (data->animations_count > 0) {
        cgltf_animation* anim = &data->animations[0];
        for (cgltf_size i = 0; i < anim->samplers_count; ++i) {
            cgltf_accessor* input = anim->samplers[i].input;
            if (input->max[0] > max_time) {
                max_time = input->max[0];
            }
        }
    }
    
    loaded = true;
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

void SkeletalAvatar::update(float delta_time) {
    if (!loaded) return;
    
    if (max_time > 0.0f) {
        current_time += delta_time;
        while (current_time > max_time) current_time -= max_time;
        
        cgltf_data* data = (cgltf_data*)gltf_data;
        if (data->animations_count > 0) {
            cgltf_animation* anim = &data->animations[0];
            
            for (cgltf_size i = 0; i < anim->channels_count; ++i) {
                cgltf_animation_channel* channel = &anim->channels[i];
                cgltf_node* node = channel->target_node;
                
                // Find bone index for this node
                int b_idx = -1;
                for (size_t b = 0; b < bones.size(); b++) {
                    if (data->skins[0].joints[b] == node) {
                        b_idx = b;
                        break;
                    }
                }
                
                if (b_idx != -1) {
                    cgltf_animation_sampler* sampler = channel->sampler;
                    cgltf_accessor* input = sampler->input;
                    cgltf_accessor* output = sampler->output;
                    
                    // Simple linear interpolation
                    for (cgltf_size k = 0; k < input->count - 1; ++k) {
                        float t0 = 0, t1 = 0;
                        cgltf_accessor_read_float(input, k, &t0, 1);
                        cgltf_accessor_read_float(input, k+1, &t1, 1);
                        
                        if (current_time >= t0 && current_time <= t1) {
                            float factor = (current_time - t0) / (t1 - t0);
                            
                            if (channel->target_path == cgltf_animation_path_type_translation) {
                                float v0[3], v1[3];
                                cgltf_accessor_read_float(output, k, v0, 3);
                                cgltf_accessor_read_float(output, k+1, v1, 3);
                                vec3 vec0(v0[0], v0[1], v0[2]);
                                vec3 vec1(v1[0], v1[1], v1[2]);
                                bones[b_idx].local_t = vec3_lerp(vec0, vec1, factor);
                            } 
                            else if (channel->target_path == cgltf_animation_path_type_rotation) {
                                float q0[4], q1[4];
                                cgltf_accessor_read_float(output, k, q0, 4);
                                cgltf_accessor_read_float(output, k+1, q1, 4);
                                quat quat0(q0[0], q0[1], q0[2], q0[3]);
                                quat quat1(q1[0], q1[1], q1[2], q1[3]);
                                bones[b_idx].local_r = quat_slerp(quat0, quat1, factor);
                            }
                            else if (channel->target_path == cgltf_animation_path_type_scale) {
                                float s0[3], s1[3];
                                cgltf_accessor_read_float(output, k, s0, 3);
                                cgltf_accessor_read_float(output, k+1, s1, 3);
                                vec3 scale0(s0[0], s0[1], s0[2]);
                                vec3 scale1(s1[0], s1[1], s1[2]);
                                bones[b_idx].local_s = vec3_lerp(scale0, scale1, factor);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Update global matrices
    for (int i = 0; i < (int)bones.size(); ++i) {
        if (bones[i].parent_index == -1) {
            update_hierarchy(i, root_transform);
        }
    }
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
        
        if (bones.size() > 0) {
            bool has_weight = false;
            for (int j = 0; j < 4; ++j) {
                if (v.weights[j] > 0.0f) {
                    mat4& joint_mat = joint_matrices[v.joints[j]];
                    vec3 local_p = mat4_mul_vec3(joint_mat, v.position);
                    final_pos = final_pos + (local_p * v.weights[j]);
                    has_weight = true;
                }
            }
            if (has_weight) {
                skinned_positions[i] = final_pos;
            } else {
                skinned_positions[i] = mat4_mul_vec3(root_transform, v.position);
            }
        } else {
            skinned_positions[i] = mat4_mul_vec3(root_transform, v.position);
        }
    }
}

void SkeletalAvatar::draw() {
    if (!loaded) return;
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glVertexPointer(3, GL_FLOAT, sizeof(vec3), skinned_positions.data());
    glTexCoordPointer(2, GL_FLOAT, sizeof(vec2), uvs.data());
    
    for (size_t i = 0; i < primitives.size(); i++) {
        Primitive& prim = primitives[i];
        
        if (prim.material_idx >= 0 && prim.material_idx < (int)materials.size()) {
            Material& mat = materials[prim.material_idx];
            glColor4fv(mat.base_color);
            if (mat.texture_id != 0) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, mat.texture_id);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            } else {
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glColor4f(0.0f, 1.0f, 0.5f, 1.0f); // Neon Green for untextured
            }
            
            if (mat.double_sided) {
                glDisable(GL_CULL_FACE);
            } else {
                glEnable(GL_CULL_FACE);
            }
        } else {
            glDisable(GL_TEXTURE_2D);
            glColor4f(0.0f, 1.0f, 0.5f, 1.0f);
            glBindTexture(GL_TEXTURE_2D, 0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_CULL_FACE);
        }
        
        glDrawElements(GL_TRIANGLES, prim.index_count, GL_UNSIGNED_INT, &indices[prim.index_offset]);
    }
    
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
}
