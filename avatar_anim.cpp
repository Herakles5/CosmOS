#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "avatar_anim.h"
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include "stb_image.h"
#include <math.h>

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
        skinned_normals.clear();
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
        }
        FILE* log = fopen("/tmp/meinos_debug.log", "a");
        if (log) {
            fprintf(log, "Loaded material %d: texture_id = %u, has_pbr = %d, tex_idx = %d\n", 
                (int)i, materials[i].texture_id, mat->has_pbr_metallic_roughness, 
                mat->has_pbr_metallic_roughness && mat->pbr_metallic_roughness.base_color_texture.texture ? 
                (int)(mat->pbr_metallic_roughness.base_color_texture.texture - data->textures) : -1);
            fclose(log);
        }
        
        printf("Loaded material %d: texture_id = %u\n", (int)i, materials[i].texture_id);

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
                
                if (color_acc) {
                    float col[4] = {1,1,1,1};
                    cgltf_accessor_read_float(color_acc, v, col, 4);
                    vert.color = vec3(col[0], col[1], col[2]);
                    colors[vertex_offset + v] = vert.color;
                } else {
                    vert.color = vec3(1,1,1);
                    colors[vertex_offset + v] = vec3(1,1,1);
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
        vec3 first_p_orig = mat4_mul_vec3(root_transform, base_vertices[0].position);
        vec3 first_p(first_p_orig.x, first_p_orig.z, -first_p_orig.y);
        vec3 min_p = first_p;
        vec3 max_p = first_p;
        for (size_t i = 1; i < base_vertices.size(); i++) {
            vec3 p_orig = mat4_mul_vec3(root_transform, base_vertices[i].position);
            // Apply -90 degree X rotation (Z-up to Y-up)
            vec3 p(p_orig.x, p_orig.z, -p_orig.y);
            
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
    
    // Reset all bones to their base local transform
    for (size_t i = 0; i < bones.size(); ++i) {
        bones[i].local_t = bones[i].base_local_t;
        bones[i].local_r = bones[i].base_local_r;
        bones[i].local_s = bones[i].base_local_s;
    }
    
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
    
    // Apply procedural animations (waving, peace sign)
    apply_procedural_animations(delta_time);
    
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
        // Transform normals (rotation only, strip translation)
        if (i < skinned_normals.size()) {
            mat4 norm_mat = root_transform;
            norm_mat.m[12] = 0; norm_mat.m[13] = 0; norm_mat.m[14] = 0;
            skinned_normals[i] = mat4_mul_vec3(norm_mat, v.normal);
        }
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
                // Textured: no lighting needed, use texture color directly
                glDisable(GL_LIGHTING);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, mat.texture_id);
                glColor4fv(mat.base_color);
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
    mat4 root_transform;
    for (int i = 0; i < (int)bones.size(); ++i) {
        if (bones[i].parent_index == -1) {
            update_hierarchy(i, root_transform);
        }
    }
}
