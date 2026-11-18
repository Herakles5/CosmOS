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
    mat4 parent_world_matrix; // World transform of non-joint parent in glTF node tree
    
    // Local transform state (current)
    vec3 local_t;
    quat local_r;
    vec3 local_s;

    // Base transform state (from bind pose)
    vec3 base_local_t;
    quat base_local_r;
    vec3 base_local_s;

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
    vec3 model_size; // Upright Y-up glTF extents (width X, height Y, depth Z)

    // Procedural animation states
    bool is_waving_left = false;
    bool is_waving_right = false;
    float wave_time = 0.0f;
    bool is_peace_sign = false;
    float peace_sign_weight = 0.0f;

    // Multi-clip & pose blend controls
    int get_animation_count() const;
    const char* get_animation_name(int idx) const;
    int get_active_clip() const { return active_anim_idx; }
    void set_animation_clip(int idx, bool reset_time = false);
    int find_animation_clip(const char* keyword) const;
    void set_anim_blend(float w) { anim_blend_weight = (w < 0.0f) ? 0.0f : (w > 1.0f ? 1.0f : w); }
    float get_anim_blend() const { return anim_blend_weight; }
    float get_max_time() const { return max_time; }
    float get_current_time() const { return current_time; }
    void set_current_time(float t);
    void reset_to_bind_pose();
    int get_bone_count() const { return (int)bones.size(); }

    void apply_procedural_animations(float delta_time);
    void add_procedural_rotation(int bone_idx, float ax, float ay, float az, float angle);
    void recompute_global_matrices();

    bool is_loaded() const { return loaded; }

    int find_bone(const char* name);
    Bone* get_bone(int index);

private:
    bool loaded;
    std::vector<AnimVertex> base_vertices;
    std::vector<vec3> skinned_positions; // For rendering
    std::vector<vec3> skinned_normals;   // For lighting
    std::vector<vec3> colors;
    std::vector<uint32_t> indices;
    
    std::vector<Primitive> primitives;
    std::vector<Material> materials;
    std::vector<vec2> uvs;
    
    std::vector<Bone> bones;
    
    // Animation data
    int active_anim_idx;
    float anim_blend_weight;
    float current_time;
    float max_time;
    
    // cgltf data pointer
    void* gltf_data; 
    
    void update_hierarchy(int bone_idx, const mat4& parent_mat);
};

#endif
