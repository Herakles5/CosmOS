#ifndef AVATAR_ANIM_H
#define AVATAR_ANIM_H

#include <stdint.h>
#include <vector>
#include <string>
#include "math3d.h"

struct AnimVertex {
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv;
    uint16_t joints[4];
    float weights[4];
};

struct Material {
    uint32_t texture_id; // OpenGL texture ID, 0 if none
    float base_color[4]; // RGBA color
    bool double_sided;
    bool alpha_blend;
    bool alpha_test;
};

struct Primitive {
    size_t index_offset;
    size_t index_count;
    int material_idx; // Index into materials array
};

struct Bone {
    std::string name;
    int parent_index;
    mat4 inverse_bind_matrix;
    
    // Local transform state
    vec3 local_t;
    quat local_r;
    vec3 local_s;

    // Global transform
    mat4 global_matrix;
};

class SkeletalAvatar {
public:
    SkeletalAvatar();
    ~SkeletalAvatar();

    bool load_glb(const char* filepath);
    void update(float delta_time);
    void skin();
    void draw();

    float auto_scale;
    vec3 center_offset;
    mat4 root_transform;

    bool is_loaded() const { return loaded; }

private:
    bool loaded;
    std::vector<AnimVertex> base_vertices;
    std::vector<vec3> skinned_positions; // For rendering
    std::vector<vec3> skinned_normals;   // For lighting
    std::vector<uint32_t> indices;
    
    std::vector<Primitive> primitives;
    std::vector<Material> materials;
    std::vector<vec2> uvs; // Temporary for rendering if we use client state, but we can also interleave or just pass base_vertices pointer
    
    std::vector<Bone> bones;
    
    // Animation data
    float current_time;
    float max_time;
    
    // cgltf data pointer
    void* gltf_data; 
    
    void update_hierarchy(int bone_idx, const mat4& parent_mat);
    int find_bone(const char* name);
    Bone* get_bone(int index);
};

#endif
