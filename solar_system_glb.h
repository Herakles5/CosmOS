#pragma once
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <vector>
#include <string>
#include "cgltf.h"
#include "stb_image.h"

struct SolarMat4 {
    float m[16];
    static SolarMat4 identity() {
        SolarMat4 r;
        memset(r.m, 0, sizeof(r.m));
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    static SolarMat4 mul(const SolarMat4& a, const SolarMat4& b) {
        SolarMat4 r;
        for (int c = 0; c < 4; c++) {
            for (int row = 0; row < 4; row++) {
                r.m[c * 4 + row] =
                    a.m[0 * 4 + row] * b.m[c * 4 + 0] +
                    a.m[1 * 4 + row] * b.m[c * 4 + 1] +
                    a.m[2 * 4 + row] * b.m[c * 4 + 2] +
                    a.m[3 * 4 + row] * b.m[c * 4 + 3];
            }
        }
        return r;
    }
    static SolarMat4 from_trs(const float t[3], const float q[4], const float s[3]) {
        float x = q[0], y = q[1], z = q[2], w = q[3];
        float x2 = x + x, y2 = y + y, z2 = z + z;
        float xx = x * x2, xy = x * y2, xz = x * z2;
        float yy = y * y2, yz = y * z2, zz = z * z2;
        float wx = w * x2, wy = w * y2, wz = w * z2;

        SolarMat4 r;
        r.m[0]  = (1.0f - (yy + zz)) * s[0];
        r.m[1]  = (xy + wz) * s[0];
        r.m[2]  = (xz - wy) * s[0];
        r.m[3]  = 0.0f;

        r.m[4]  = (xy - wz) * s[1];
        r.m[5]  = (1.0f - (xx + zz)) * s[1];
        r.m[6]  = (yz + wx) * s[1];
        r.m[7]  = 0.0f;

        r.m[8]  = (xz + wy) * s[2];
        r.m[9]  = (yz - wx) * s[2];
        r.m[10] = (1.0f - (xx + yy)) * s[2];
        r.m[11] = 0.0f;

        r.m[12] = t[0];
        r.m[13] = t[1];
        r.m[14] = t[2];
        r.m[15] = 1.0f;
        return r;
    }
};

struct SolarMaterial {
    GLuint texture_id = 0;
    float base_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float emissive[3] = {0.0f, 0.0f, 0.0f};
    bool is_orbit_ring = false;
    bool is_sun = false;
};

struct SolarMeshPrim {
    GLuint display_list = 0;
    int material_idx = -1;
};

struct SolarMesh {
    std::vector<SolarMeshPrim> prims;
};

struct SolarNode {
    std::string name;
    int parent_idx = -1;
    std::vector<int> children;
    int mesh_idx = -1;
    bool has_matrix = false;
    SolarMat4 base_matrix;
    float base_t[3] = {0.0f, 0.0f, 0.0f};
    float base_r[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float base_s[3] = {1.0f, 1.0f, 1.0f};

    float cur_t[3] = {0.0f, 0.0f, 0.0f};
    float cur_r[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float cur_s[3] = {1.0f, 1.0f, 1.0f};
    SolarMat4 world_matrix;
    bool is_planet_body = false;
};

struct SolarAnimChannel {
    int node_idx = -1;
    cgltf_animation_path_type path = cgltf_animation_path_type_invalid;
    std::vector<float> times;
    std::vector<float> values; // 3 floats per key for T/S, 4 floats per key for R
};

class SolarSystemGLB {
public:
    bool loaded = false;
    float anim_time = 0.0f;
    float max_time = 20.0f;
    float min_time = 0.0333333f;
    std::vector<SolarMaterial> materials;
    std::vector<SolarMesh> meshes;
    std::vector<SolarNode> nodes;
    std::vector<int> root_nodes;
    std::vector<SolarAnimChannel> channels;

    bool load(const char* filepath) {
        cgltf_options options;
        memset(&options, 0, sizeof(options));
        cgltf_data* data = nullptr;
        cgltf_result res = cgltf_parse_file(&options, filepath, &data);
        if (res != cgltf_result_success) {
            printf("[SOLAR_GLB] Failed to parse %s\n", filepath);
            return false;
        }
        res = cgltf_load_buffers(&options, data, filepath);
        if (res != cgltf_result_success) {
            printf("[SOLAR_GLB] Failed to load buffers for %s\n", filepath);
            cgltf_free(data);
            return false;
        }

        // 1. Load textures
        std::vector<GLuint> gl_textures(data->textures_count, 0);
        for (cgltf_size i = 0; i < data->textures_count; i++) {
            cgltf_texture* tex = &data->textures[i];
            if (tex->image && tex->image->buffer_view && tex->image->buffer_view->buffer->data) {
                const uint8_t* ptr = (const uint8_t*)tex->image->buffer_view->buffer->data + tex->image->buffer_view->offset;
                size_t size = tex->image->buffer_view->size;
                int w = 0, h = 0, comp = 0;
                unsigned char* pixels = stbi_load_from_memory(ptr, (int)size, &w, &h, &comp, 4);
                if (pixels) {
                    GLuint tid = 0;
                    glGenTextures(1, &tid);
                    glBindTexture(GL_TEXTURE_2D, tid);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                    stbi_image_free(pixels);
                    gl_textures[i] = tid;
                }
            }
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        // 2. Load materials
        materials.resize(data->materials_count);
        for (cgltf_size i = 0; i < data->materials_count; i++) {
            cgltf_material* mat = &data->materials[i];
            SolarMaterial& sm = materials[i];
            if (mat->has_pbr_metallic_roughness) {
                for (int k = 0; k < 4; k++) sm.base_color[k] = mat->pbr_metallic_roughness.base_color_factor[k];
                if (mat->pbr_metallic_roughness.base_color_texture.texture) {
                    int tidx = (int)(mat->pbr_metallic_roughness.base_color_texture.texture - data->textures);
                    if (tidx >= 0 && tidx < (int)gl_textures.size()) sm.texture_id = gl_textures[tidx];
                }
            }
            for (int k = 0; k < 3; k++) sm.emissive[k] = mat->emissive_factor[k];
            if (mat->emissive_texture.texture && sm.texture_id == 0) {
                int tidx = (int)(mat->emissive_texture.texture - data->textures);
                if (tidx >= 0 && tidx < (int)gl_textures.size()) sm.texture_id = gl_textures[tidx];
            }
            if (mat->name && strcmp(mat->name, "Material") == 0) {
                sm.is_orbit_ring = true;
            }
            if (mat->name && strcmp(mat->name, "material") == 0) {
                sm.is_sun = true;
            }
        }

        // 3. Compile meshes into hardware OpenGL Display Lists (uploaded to GPU once!)
        meshes.resize(data->meshes_count);
        for (cgltf_size i = 0; i < data->meshes_count; i++) {
            cgltf_mesh* m = &data->meshes[i];
            meshes[i].prims.resize(m->primitives_count);
            for (cgltf_size p = 0; p < m->primitives_count; p++) {
                cgltf_primitive* prim = &m->primitives[p];
                SolarMeshPrim& sp = meshes[i].prims[p];
                if (prim->material) {
                    sp.material_idx = (int)(prim->material - data->materials);
                }
                cgltf_accessor* pos_acc = nullptr;
                cgltf_accessor* norm_acc = nullptr;
                cgltf_accessor* uv_acc = nullptr;
                for (cgltf_size a = 0; a < prim->attributes_count; a++) {
                    if (prim->attributes[a].type == cgltf_attribute_type_position) pos_acc = prim->attributes[a].data;
                    else if (prim->attributes[a].type == cgltf_attribute_type_normal) norm_acc = prim->attributes[a].data;
                    else if (prim->attributes[a].type == cgltf_attribute_type_texcoord) uv_acc = prim->attributes[a].data;
                }
                if (!pos_acc) continue;

                size_t vcount = pos_acc->count;
                std::vector<float> pos(vcount * 3, 0.0f);
                std::vector<float> norm(vcount * 3, 0.0f);
                std::vector<float> uv(vcount * 2, 0.0f);
                for (size_t v = 0; v < vcount; v++) {
                    cgltf_accessor_read_float(pos_acc, v, &pos[v * 3], 3);
                    if (norm_acc) cgltf_accessor_read_float(norm_acc, v, &norm[v * 3], 3);
                    if (uv_acc) cgltf_accessor_read_float(uv_acc, v, &uv[v * 2], 2);
                }

                sp.display_list = glGenLists(1);
                glNewList(sp.display_list, GL_COMPILE);
                glBegin(GL_TRIANGLES);
                if (prim->indices) {
                    size_t icount = prim->indices->count;
                    for (size_t idx = 0; idx < icount; idx++) {
                        cgltf_uint vi = 0;
                        cgltf_accessor_read_uint(prim->indices, idx, &vi, 1);
                        if (vi < vcount) {
                            if (uv_acc) glTexCoord2f(uv[vi * 2 + 0], uv[vi * 2 + 1]);
                            if (norm_acc) glNormal3f(norm[vi * 3 + 0], norm[vi * 3 + 1], norm[vi * 3 + 2]);
                            glVertex3f(pos[vi * 3 + 0], pos[vi * 3 + 1], pos[vi * 3 + 2]);
                        }
                    }
                } else {
                    for (size_t vi = 0; vi < vcount; vi++) {
                        if (uv_acc) glTexCoord2f(uv[vi * 2 + 0], uv[vi * 2 + 1]);
                        if (norm_acc) glNormal3f(norm[vi * 3 + 0], norm[vi * 3 + 1], norm[vi * 3 + 2]);
                        glVertex3f(pos[vi * 3 + 0], pos[vi * 3 + 1], pos[vi * 3 + 2]);
                    }
                }
                glEnd();
                glEndList();
            }
        }

        // 4. Load nodes & hierarchy
        nodes.resize(data->nodes_count);
        for (cgltf_size i = 0; i < data->nodes_count; i++) {
            cgltf_node* cn = &data->nodes[i];
            SolarNode& sn = nodes[i];
            sn.name = cn->name ? cn->name : "";
            if (cn->parent) sn.parent_idx = (int)(cn->parent - data->nodes);
            else root_nodes.push_back((int)i);

            for (cgltf_size c = 0; c < cn->children_count; c++) {
                sn.children.push_back((int)(cn->children[c] - data->nodes));
            }
            if (cn->mesh) sn.mesh_idx = (int)(cn->mesh - data->meshes);

            if (cn->has_matrix) {
                sn.has_matrix = true;
                memcpy(sn.base_matrix.m, cn->matrix, sizeof(float) * 16);
                // Boost planet body visibility slightly (keeping orbital position m[12..14] intact)
                if (sn.name.find("mercury_2") != std::string::npos ||
                    sn.name.find("venus_5") != std::string::npos ||
                    sn.name.find("erath_8") != std::string::npos ||
                    sn.name.find("mars_12") != std::string::npos ||
                    sn.name.find("jupiter_15") != std::string::npos ||
                    sn.name.find("saturn_19") != std::string::npos ||
                    sn.name.find("uranus_22") != std::string::npos ||
                    sn.name.find("neptune_25") != std::string::npos ||
                    sn.name.find("pluto_28") != std::string::npos ||
                    sn.name.find("moon_31") != std::string::npos) {
                    sn.is_planet_body = true;
                    float scale_boost = (sn.name.find("erath_8") != std::string::npos || sn.name.find("moon_31") != std::string::npos) ? 1.25f : 1.55f;
                    for (int col = 0; col < 3; col++) {
                        for (int row = 0; row < 3; row++) {
                            sn.base_matrix.m[col * 4 + row] *= scale_boost;
                        }
                    }
                }
            } else {
                if (cn->has_translation) memcpy(sn.base_t, cn->translation, sizeof(float) * 3);
                if (cn->has_rotation) memcpy(sn.base_r, cn->rotation, sizeof(float) * 4);
                if (cn->has_scale) memcpy(sn.base_s, cn->scale, sizeof(float) * 3);
                memcpy(sn.cur_t, sn.base_t, sizeof(float) * 3);
                memcpy(sn.cur_r, sn.base_r, sizeof(float) * 4);
                memcpy(sn.cur_s, sn.base_s, sizeof(float) * 3);
            }
        }

        // 5. Load initial orbital inclination quaternions from first animation keyframe
        if (data->animations_count > 0) {
            cgltf_animation* anim = &data->animations[0];
            min_time = 1e9f;
            max_time = 0.0f;
            for (cgltf_size c = 0; c < anim->channels_count; c++) {
                cgltf_animation_channel* ch = &anim->channels[c];
                if (!ch->target_node || !ch->sampler || !ch->sampler->input || !ch->sampler->output) continue;
                SolarAnimChannel sac;
                sac.node_idx = (int)(ch->target_node - data->nodes);
                sac.path = ch->target_path;
                size_t kcount = ch->sampler->input->count;
                sac.times.resize(kcount);
                int num_comp = (sac.path == cgltf_animation_path_type_rotation) ? 4 : 3;
                sac.values.resize(kcount * num_comp);
                for (size_t k = 0; k < kcount; k++) {
                    cgltf_accessor_read_float(ch->sampler->input, k, &sac.times[k], 1);
                    cgltf_accessor_read_float(ch->sampler->output, k, &sac.values[k * num_comp], num_comp);
                    if (sac.times[k] < min_time) min_time = sac.times[k];
                    if (sac.times[k] > max_time) max_time = sac.times[k];
                }
                // Store k=0 orbital inclination quaternion (q_tilt) into node's base_r & cur_r
                if (sac.path == cgltf_animation_path_type_rotation && kcount > 0 &&
                    sac.node_idx >= 0 && sac.node_idx < (int)nodes.size()) {
                    memcpy(nodes[sac.node_idx].base_r, &sac.values[0], sizeof(float) * 4);
                    memcpy(nodes[sac.node_idx].cur_r, &sac.values[0], sizeof(float) * 4);
                }
                channels.push_back(sac);
            }
        }

        cgltf_free(data);
        loaded = true;
        printf("[SOLAR_GLB] Successfully loaded %s (%zu nodes, %zu meshes, %zu anim channels)\n",
               filepath, nodes.size(), meshes.size(), channels.size());
        return true;
    }

    static void quat_slerp(const float q0[4], const float q1[4], float t, float out[4]) {
        float cosom = q0[0]*q1[0] + q0[1]*q1[1] + q0[2]*q1[2] + q0[3]*q1[3];
        float q1c[4] = {q1[0], q1[1], q1[2], q1[3]};
        if (cosom < 0.0f) {
            cosom = -cosom;
            for (int i = 0; i < 4; i++) q1c[i] = -q1c[i];
        }
        float scale0, scale1;
        if (1.0f - cosom > 0.0001f) {
            float omega = acosf(cosom);
            float sinom = sinf(omega);
            scale0 = sinf((1.0f - t) * omega) / sinom;
            scale1 = sinf(t * omega) / sinom;
        } else {
            scale0 = 1.0f - t;
            scale1 = t;
        }
        for (int i = 0; i < 4; i++) out[i] = scale0 * q0[i] + scale1 * q1c[i];
    }

    static void quat_mul(const float a[4], const float b[4], float out[4]) {
        // Quaternion multiplication: out = a * b (x, y, z, w)
        out[0] = a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1];
        out[1] = a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0];
        out[2] = a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3];
        out[3] = a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2];
    }

    void update(float dt) {
        if (!loaded) return;
        anim_time += dt;

        // 1. Real-Time Astronomical Ephemeris (J2000.0 NASA/JPL Heliocentric Mean Longitudes)
        // Epoch J2000.0 = 2000-01-01 12:00:00 UTC (Unix timestamp 946728000.0)
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double unix_sec = (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
        double d_days = (unix_sec - 946728000.0) / 86400.0;

        struct OrbitEphemeris {
            const char* node_prefix;
            double L0_deg;          // Mean longitude at J2000.0 (degrees)
            double rate_deg_day;    // Daily orbital motion (degrees / day)
        };
        static const OrbitEphemeris kEphemeris[] = {
            { "mercury_BezierCircle_", 252.2509,  4.09233445 }, // 87.969 days
            { "venus_BezierCircle_",   181.9798,  1.60213034 }, // 224.701 days
            { "erath_BezierCircle_",   100.4664,  0.98560911 }, // 365.256 days (Earth)
            { "mars_BezierCircle_",    355.4330,  0.52403295 }, // 686.980 days
            { "jupiter_BezierCircle_",  34.3515,  0.08309122 }, // 4332.589 days
            { "saturn_BezierCircle_",   50.0774,  0.03345965 }, // 10759.22 days
            { "uranus_BezierCircle_",  314.0550,  0.01173129 }, // 30685.4 days
            { "neptune_BezierCircle_", 304.3487,  0.00598103 }, // 60189.0 days
            { "pluto_BezierCircle_",   238.9290,  0.00396400 }, // 90560.0 days
            { "moon_BezierCircle_",    218.3160, 13.17639600 }  // 27.32166 days (Moon around Earth)
        };

        int earth_orbit_node = -1;
        int earth_body_node  = -1;
        int moon_orbit_node  = -1;
        int moon_ring_node   = -1;

        for (size_t i = 0; i < nodes.size(); i++) {
            SolarNode& sn = nodes[i];
            if (sn.name == "erath_BezierCircle_11") earth_orbit_node = (int)i;
            else if (sn.name == "erath_8") earth_body_node = (int)i;
            else if (sn.name == "moon_BezierCircle_33") moon_orbit_node = (int)i;
            else if (sn.name == "moon_BezierCircle.001_43") moon_ring_node = (int)i;

            // Continuous axial rotation of the Sun ('sun_53')
            if (sn.name == "sun_53") {
                float sun_angle = fmodf(anim_time * 0.45f, 6.2831853f);
                sn.cur_r[0] = 0.0f;
                sn.cur_r[1] = sinf(sun_angle * 0.5f);
                sn.cur_r[2] = 0.0f;
                sn.cur_r[3] = cosf(sun_angle * 0.5f);
                continue;
            }

            // Skip orbital ring meshes (*.001_*) so their static inclination rings remain fixed
            if (sn.name.find(".001_") != std::string::npos) continue;

            for (size_t e = 0; e < sizeof(kEphemeris) / sizeof(kEphemeris[0]); e++) {
                if (sn.name.find(kEphemeris[e].node_prefix) == 0) {
                    double lon_deg = fmod(kEphemeris[e].L0_deg + kEphemeris[e].rate_deg_day * d_days, 360.0);
                    if (lon_deg < 0.0) lon_deg += 360.0;
                    float lon_rad = (float)(lon_deg * (3.14159265358979323846 / 180.0));
                    float q_orbit[4] = { 0.0f, sinf(lon_rad * 0.5f), 0.0f, cosf(lon_rad * 0.5f) };
                    quat_mul(sn.base_r, q_orbit, sn.cur_r);
                    break;
                }
            }
        }

        // Keep Moon ('moon_BezierCircle_33') and Moon Orbit Ring ('moon_BezierCircle.001_43')
        // centered on Earth's exact real-time 3D position
        if (earth_orbit_node >= 0 && earth_body_node >= 0) {
            const SolarNode& eo = nodes[earth_orbit_node];
            const SolarNode& eb = nodes[earth_body_node];
            SolarMat4 eo_mat = SolarMat4::from_trs(eo.cur_t, eo.cur_r, eo.cur_s);
            SolarMat4 eb_mat = SolarMat4::mul(eo_mat, eb.base_matrix);
            if (moon_orbit_node >= 0) {
                nodes[moon_orbit_node].cur_t[0] = eb_mat.m[12];
                nodes[moon_orbit_node].cur_t[1] = eb_mat.m[13];
                nodes[moon_orbit_node].cur_t[2] = eb_mat.m[14];
            }
            if (moon_ring_node >= 0) {
                nodes[moon_ring_node].cur_t[0] = eb_mat.m[12];
                nodes[moon_ring_node].cur_t[1] = eb_mat.m[13];
                nodes[moon_ring_node].cur_t[2] = eb_mat.m[14];
            }
        }

        // Propagate hierarchy transforms
        for (size_t i = 0; i < root_nodes.size(); i++) {
            update_node_recursive(root_nodes[i], SolarMat4::identity());
        }
    }

    void update_node_recursive(int idx, const SolarMat4& parent_mat) {
        SolarNode& sn = nodes[idx];
        SolarMat4 local_mat = sn.has_matrix ? sn.base_matrix : SolarMat4::from_trs(sn.cur_t, sn.cur_r, sn.cur_s);
        sn.world_matrix = SolarMat4::mul(parent_mat, local_mat);
        for (size_t c = 0; c < sn.children.size(); c++) {
            update_node_recursive(sn.children[c], sn.world_matrix);
        }
    }

    void render(int win_w, int win_h, float cam_fov, float cam_z_offset, float cam_rot_x, float cam_rot_y, bool show_planets) {
        if (!loaded) return;

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glViewport(0, 0, win_w, win_h);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        float near_p = 1.0f;
        float far_p = 40000.0f;
        float right_p = near_p * (float)win_w / (2.0f * cam_fov);
        float top_p = near_p * (float)win_h / (2.0f * cam_fov);
        glFrustum(-right_p, right_p, -top_p, top_p, near_p, far_p);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // Match CosmOS interactive 3D camera convention (with 22-deg default orbital plane tilt)
        glTranslatef(0.0f, 0.0f, -cam_z_offset);
        glRotatef(22.0f + cam_rot_x * 57.2957795f, 1.0f, 0.0f, 0.0f);
        glRotatef(-cam_rot_y * 57.2957795f, 0.0f, 1.0f, 0.0f);

        // Scale GLB scene so all planets + Saturn rings + outer orbits span the 1080p viewport majestically
        float scene_scale = 18.5f;
        glScalef(scene_scale, scene_scale, scene_scale);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_NORMALIZE);

        // Point light at the center of the Sun (0, 0, 0, 1)
        GLfloat light_pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        GLfloat light_ambient[4] = {0.48f, 0.48f, 0.52f, 1.0f};
        GLfloat light_diffuse[4] = {1.15f, 1.08f, 0.96f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

        // Pass 1: Opaque meshes (Sun + Planets + Saturn Ring)
        for (size_t i = 0; i < nodes.size(); i++) {
            const SolarNode& sn = nodes[i];
            if (sn.mesh_idx < 0 || sn.mesh_idx >= (int)meshes.size()) continue;
            const SolarMesh& mesh = meshes[sn.mesh_idx];
            for (size_t p = 0; p < mesh.prims.size(); p++) {
                const SolarMeshPrim& prim = mesh.prims[p];
                if (prim.display_list == 0) continue;
                const SolarMaterial* mat = (prim.material_idx >= 0 && prim.material_idx < (int)materials.size())
                                           ? &materials[prim.material_idx] : nullptr;
                if (mat && mat->is_orbit_ring) continue; // Drawn in Pass 2
                if (!show_planets && (!mat || !mat->is_sun)) continue;

                glPushMatrix();
                glMultMatrixf(sn.world_matrix.m);

                if (mat && mat->is_sun) {
                    glDisable(GL_LIGHTING);
                    glColor4f(1.0f, 0.96f, 0.88f, 1.0f);
                } else {
                    glEnable(GL_LIGHTING);
                    if (mat) glColor4fv(mat->base_color);
                    else glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                }

                if (mat && mat->texture_id != 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, mat->texture_id);
                } else {
                    glDisable(GL_TEXTURE_2D);
                }

                glCallList(prim.display_list);
                glPopMatrix();
            }
        }

        // Pass 2: Translucent Orbital Rings
        if (show_planets) {
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glColor4f(0.25f, 0.85f, 0.95f, 0.28f);

            for (size_t i = 0; i < nodes.size(); i++) {
                const SolarNode& sn = nodes[i];
                if (sn.mesh_idx < 0 || sn.mesh_idx >= (int)meshes.size()) continue;
                const SolarMesh& mesh = meshes[sn.mesh_idx];
                for (size_t p = 0; p < mesh.prims.size(); p++) {
                    const SolarMeshPrim& prim = mesh.prims[p];
                    if (prim.display_list == 0) continue;
                    const SolarMaterial* mat = (prim.material_idx >= 0 && prim.material_idx < (int)materials.size())
                                               ? &materials[prim.material_idx] : nullptr;
                    if (!mat || !mat->is_orbit_ring) continue;

                    glPushMatrix();
                    glMultMatrixf(sn.world_matrix.m);
                    glCallList(prim.display_list);
                    glPopMatrix();
                }
            }
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();
    }
};
