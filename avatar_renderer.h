#pragma once
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <sys/stat.h>
#include <vector>
#include <SDL2/SDL.h>

extern "C" uint32_t aion_window_x;
extern "C" uint32_t aion_window_y;
extern "C" uint32_t aion_window_w;
extern "C" uint32_t aion_window_h;
extern "C" uint8_t  aion_window_open;
extern "C" uint8_t  aion_use_3d_avatar;
extern uint32_t linux_current_freq;
// Lock removed - 3D interaction is always active
float aion_avatar_zoom = 0.6f;
float aion_avatar_rot_x = 0.0f;
float aion_avatar_rot_y = 0.0f;
float aion_avatar_rot_z = 0.0f;
float aion_avatar_pan_x = 0.0f;
float aion_avatar_pan_y = 0.0f;
int aion_avatar_screen_x = 0;
int aion_avatar_screen_y = 0;
int jaw_idx = -2;
int jaw_is_cat_z = 0;
int tongue1_idx = -2;
int tongue2_idx = -2;
int whisker_l_idx = -2;
int whisker_r_idx = -2;
int ear_l_idx = -2;
int ear_r_idx = -2;
int head_bone_idx = -2;
int spine_idx = -2;
int arm_r_idx = -2;
int forearm_r_idx = -2;
int hand_r_idx = -2;
int arm_l_idx = -2;
int forearm_l_idx = -2;
int hand_l_idx = -2;
int tail0_idx = -2;
int tail1_idx = -2;
int tail2_idx = -2;
float anim_time = 0.0f;
#include "avatar_anim.h"

SkeletalAvatar global_avatar;

extern "C" uint8_t  aion_switch_avatar = 0;
extern "C" int      aion_current_avatar_idx = 12; // Model 13/13 (Tuxedo Cat) as default main character
extern "C" int      aion_total_models = 0;

// --- Real-Time WAV Speech Envelope Analyzer for Synchronous Lip-Sync ---
static std::vector<int16_t> g_speech_pcm;
static uint32_t g_speech_sample_rate = 22050;
static time_t g_speech_wav_mtime = 0;
static uint32_t g_speech_start_ticks = 0;
static bool g_was_speaking = false;
static float g_smooth_mouth_open = 0.0f;

static void reload_speech_wav_if_needed() {
    struct stat st;
    if (stat("/tmp/aion_response.wav", &st) != 0) return;
    if (st.st_mtime == g_speech_wav_mtime && !g_speech_pcm.empty()) return;

    FILE* f = fopen("/tmp/aion_response.wav", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 44 || fsize > 32 * 1024 * 1024) {
        fclose(f);
        return;
    }

    std::vector<uint8_t> raw(fsize);
    if (fread(raw.data(), 1, fsize, f) != (size_t)fsize) {
        fclose(f);
        return;
    }
    fclose(f);

    uint32_t sr = 22050;
    uint16_t channels = 1;
    memcpy(&channels, &raw[22], 2);
    memcpy(&sr, &raw[24], 4);
    if (sr < 8000 || sr > 96000) sr = 22050;
    if (channels < 1 || channels > 2) channels = 1;
    g_speech_sample_rate = sr;

    // Locate "data" chunk offset
    size_t data_offset = 44;
    for (size_t i = 12; i + 8 < (size_t)fsize && i < 256; ++i) {
        if (raw[i] == 'd' && raw[i+1] == 'a' && raw[i+2] == 't' && raw[i+3] == 'a') {
            data_offset = i + 8;
            break;
        }
    }

    size_t num_samples = (fsize - data_offset) / (2 * channels);
    g_speech_pcm.resize(num_samples);
    const int16_t* src = reinterpret_cast<const int16_t*>(raw.data() + data_offset);
    for (size_t i = 0; i < num_samples; ++i) {
        g_speech_pcm[i] = src[i * channels];
    }
    g_speech_wav_mtime = st.st_mtime;
}

static float compute_realtime_speech_envelope(bool is_speaking) {
    uint32_t now_ticks = SDL_GetTicks();
    if (is_speaking) {
        if (!g_was_speaking) {
            reload_speech_wav_if_needed();
            g_speech_start_ticks = now_ticks;
            g_was_speaking = true;
        } else {
            // Check if a new WAV was generated while speaking
            struct stat st;
            if (stat("/tmp/aion_response.wav", &st) == 0 && st.st_mtime != g_speech_wav_mtime) {
                reload_speech_wav_if_needed();
                g_speech_start_ticks = now_ticks;
            }
        }
    } else {
        g_was_speaking = false;
    }

    float target_open = 0.0f;
    if (is_speaking && !g_speech_pcm.empty()) {
        uint32_t elapsed_ms = now_ticks - g_speech_start_ticks;
        size_t center_sample = (size_t)((uint64_t)elapsed_ms * g_speech_sample_rate / 1000ULL);
        size_t window_samples = g_speech_sample_rate / 30; // ~33ms syllable window
        if (center_sample < g_speech_pcm.size()) {
            size_t end_s = center_sample + window_samples;
            if (end_s > g_speech_pcm.size()) end_s = g_speech_pcm.size();
            double sum_sq = 0.0;
            int32_t peak = 0;
            size_t count = end_s - center_sample;
            for (size_t i = center_sample; i < end_s; ++i) {
                int32_t v = g_speech_pcm[i];
                int32_t av = (v < 0) ? -v : v;
                if (av > peak) peak = av;
                sum_sq += (double)(v * v);
            }
            float rms = (count > 0) ? sqrtf((float)(sum_sq / count)) / 32768.0f : 0.0f;
            float pk  = (float)peak / 32768.0f;
            // Noise gate + dynamic syllable compression for crisp lip-sync
            float env = (rms * 3.8f + pk * 0.9f);
            if (env < 0.04f) env = 0.0f;
            else env = (env - 0.04f) / 0.55f;
            if (env > 1.0f) env = 1.0f;
            // Subtle syllable micro-articulation
            float micro = 0.82f + 0.18f * sinf(anim_time * 28.0f);
            target_open = env * micro;
        } else {
            // Fallback syllable cadence if pw-play is still finishing buffer
            float cad = sinf(anim_time * 16.0f) * 0.5f + sinf(anim_time * 25.0f) * 0.35f + 0.25f;
            target_open = (cad > 0.0f) ? cad : 0.0f;
        }
    } else if (is_speaking) {
        float cad = sinf(anim_time * 16.0f) * 0.5f + sinf(anim_time * 25.0f) * 0.35f + 0.25f;
        target_open = (cad > 0.0f) ? cad : 0.0f;
    }

    // Fast attack when vowel starts, smooth natural close when syllable ends
    float rate = (target_open > g_smooth_mouth_open) ? 0.52f : 0.28f;
    g_smooth_mouth_open += (target_open - g_smooth_mouth_open) * rate;
    if (g_smooth_mouth_open < 0.005f) g_smooth_mouth_open = 0.0f;
    return g_smooth_mouth_open;
}

extern "C" void aion_save_config(int idx) {
    char path[256];
    snprintf(path, 256, "/opt/meinos/avatar_config_%d.bin", idx);
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(&aion_avatar_rot_x, sizeof(float), 1, f);
        fwrite(&aion_avatar_rot_y, sizeof(float), 1, f);
        fwrite(&aion_avatar_rot_z, sizeof(float), 1, f);
        fwrite(&aion_avatar_pan_x, sizeof(float), 1, f);
        fwrite(&aion_avatar_pan_y, sizeof(float), 1, f);
        fwrite(&aion_avatar_zoom, sizeof(float), 1, f);
        fclose(f);
    }
}

extern "C" void aion_load_config(int idx) {
    char path[256];
    snprintf(path, 256, "/opt/meinos/avatar_config_%d.bin", idx);
    FILE* f = fopen(path, "rb");
    if (f) {
        fread(&aion_avatar_rot_x, sizeof(float), 1, f);
        fread(&aion_avatar_rot_y, sizeof(float), 1, f);
        fread(&aion_avatar_rot_z, sizeof(float), 1, f);
        fread(&aion_avatar_pan_x, sizeof(float), 1, f);
        fread(&aion_avatar_pan_y, sizeof(float), 1, f);
        fread(&aion_avatar_zoom, sizeof(float), 1, f);
        fclose(f);
    } else {
        // Defaults
        aion_avatar_rot_x = 0.0f;
        aion_avatar_rot_y = 0.0f;
        aion_avatar_rot_z = 0.0f;
        aion_avatar_pan_x = 0.0f;
        aion_avatar_pan_y = 0.0f;
        aion_avatar_zoom = 0.6f;
    }
}

void load_a3d(const char* path) {
    global_avatar.load_glb(path);
    if (global_avatar.is_loaded()) {
        printf("Successfully loaded GLB model %s\n", path);
    } else {
        printf("Failed to load GLB model %s\n", path);
    }
}

static float aion_follow_rot_x = 0.0f;
static float aion_follow_rot_y = 0.0f;
bool aion_mouse_dragging = false;
static uint64_t aion_drag_release_time = 0;

// --- Autonomous Desktop Roaming, Walking/Running & Paw-Pointing State ---
static int   g_roam_mode = 0;          // 0 = Follow Mouse/Patrol Walk, 1 = Desktop Run/Trot, 2 = Paw-Point at Desktop Item
static float g_roam_timer = 4.5f;
static float g_roam_wp_x = 0.0f;
static float g_roam_wp_y = 0.0f;
static int   g_point_paw_side = 0;     // 0 = Right Paw, 1 = Left Paw
static float g_point_blend = 0.0f;
static uint32_t g_last_mouse_x = 0;
static uint32_t g_last_mouse_y = 0;
static int   g_mouse_idle_frames = 0;

void draw_a3d() {
    if (aion_switch_avatar) {
        aion_switch_avatar = 0;
        char path[256];
        snprintf(path, 256, "/opt/meinos/model_%d.glb", aion_current_avatar_idx);
        load_a3d(path);
        aion_load_config(aion_current_avatar_idx);
        // Reset bone indices so it searches for the correct bones in the new model
        jaw_idx = -2;
        jaw_is_cat_z = 0;
        tongue1_idx = -2;
        tongue2_idx = -2;
        whisker_l_idx = -2;
        whisker_r_idx = -2;
        ear_l_idx = -2;
        ear_r_idx = -2;
        head_bone_idx = -2;
        spine_idx = -2;
        arm_l_idx = -2;
        forearm_l_idx = -2;
        hand_l_idx = -2;
        arm_r_idx = -2;
        forearm_r_idx = -2;
        hand_r_idx = -2;
        tail0_idx = -2;
        tail1_idx = -2;
        tail2_idx = -2;
    }

    // Render AION on the desktop whether AION NEXUS is open, minimized, or closed!
    if (!global_avatar.is_loaded() || !aion_use_3d_avatar) return;
    
    extern int global_screen_w;
    extern int global_screen_h;
    extern uint32_t mouse_x;
    extern uint32_t mouse_y;
    extern bool mouse_down;
    
    // --- Smooth Mouse Follow & Desktop Anchor ---
    uint32_t eff_win_x = (aion_window_w > 0) ? aion_window_x : 1450;
    uint32_t eff_win_y = (aion_window_h > 0) ? aion_window_y : 28;
    uint32_t eff_win_w = (aion_window_w > 0) ? aion_window_w : 450;
    uint32_t eff_win_h = (aion_window_h > 0) ? aion_window_h : 500;

    float model_center_x = eff_win_x + eff_win_w * 0.5f;
    float model_center_y = eff_win_y + 30 + (eff_win_h - 220 - 30) * 0.5f;
    
    // Frustum dimensions at Z = -3.0f (must match the glFrustum setup below)
    float aspect = (float)global_screen_w / (float)global_screen_h;
    float fov = 45.0f * (3.14159f / 180.0f);
    float half_h_at_z3 = tanf(fov / 2.0f) * 3.0f;
    float half_w_at_z3 = half_h_at_z3 * aspect;
    
    // Calculate AION anchor center in normalized screen coordinates
    float nx = (model_center_x / (float)global_screen_w) * 2.0f - 1.0f;
    float ny = 1.0f - (model_center_y / (float)global_screen_h) * 2.0f;
    float baseline_x = nx * half_w_at_z3;
    float baseline_y = ny * half_h_at_z3;

    // Calculate avatar screen coordinates for her interactive desktop hitbox
    float current_world_x = baseline_x + aion_avatar_pan_x;
    float current_world_y = baseline_y - 0.75f + aion_avatar_pan_y;
    float current_nx = current_world_x / half_w_at_z3;
    float current_ny = current_world_y / half_h_at_z3;
    aion_avatar_screen_x = (int)((current_nx + 1.0f) * 0.5f * global_screen_w);
    aion_avatar_screen_y = (int)((1.0f - current_ny) * 0.5f * global_screen_h);

    // Check if speaking
    FILE* sf = fopen("/tmp/aion_speaking", "r");
    bool is_speaking = false;
    if (sf) {
        is_speaking = true;
        fclose(sf);
    }

    // Decoupled from mouse: AION moves 100% autonomously across the desktop
    aion_mouse_dragging = false;
    aion_drag_release_time = 0;

    // --- 100% Autonomous Desktop Roaming (Completely Decoupled from Mouse Movement!) ---
    g_roam_timer -= 0.016f;
    if (g_roam_timer <= 0.0f) {
        // Cycle behavior between Desktop Patrol Walk, Energetic Run, and Paw-Pointing at desktop items
        int r = rand() % 100;
        if (is_speaking || r < 32) {
            g_roam_mode = 2; // Pause & point with paw at a desktop object / planet / icon
            g_roam_timer = 3.0f + (rand() % 25) * 0.1f;
            g_point_paw_side = rand() % 2;
        } else if (r < 72) {
            g_roam_mode = 0; // Autonomous walk across desktop
            g_roam_timer = 4.8f + (rand() % 35) * 0.1f;
        } else {
            g_roam_mode = 1; // Autonomous energetic run/trot across desktop
            g_roam_timer = 3.2f + (rand() % 25) * 0.1f;
        }

        // Autonomous desktop patrol waypoints across the 1920x1080 screen (never tied to mouse_x/mouse_y)
        static const float patrol_pts[8][2] = {
            {  0.0f,  0.0f },   // Solar System Sun Center
            { -1.15f, 0.35f },  // Top-Left Desktop Sector
            {  1.05f,-0.32f },  // Bottom-Right Notification Sector
            { -0.85f,-0.42f },  // Bottom-Left Dock Sector
            {  0.90f, 0.38f },  // Top-Right AION Nexus Sector
            {  0.0f, -0.45f },  // Bottom Dock Center
            { -0.55f, 0.18f },  // Inner Planetary Orbit Left
            {  0.60f, 0.15f }   // Inner Planetary Orbit Right
        };
        int pt = rand() % 8;
        float jitter_x = ((rand() % 100) / 100.0f - 0.5f) * 0.35f;
        float jitter_y = ((rand() % 100) / 100.0f - 0.5f) * 0.25f;
        g_roam_wp_x = (patrol_pts[pt][0] + jitter_x) * half_w_at_z3 * 0.65f;
        g_roam_wp_y = (patrol_pts[pt][1] + jitter_y) * half_h_at_z3 * 0.72f;

        // Clamp desktop waypoints so AION stays well inside the visible 1920x1080 screen
        float max_wx = half_w_at_z3 * 0.65f;
        float max_wy = half_h_at_z3 * 0.45f;
        if (g_roam_wp_x < -max_wx) g_roam_wp_x = -max_wx;
        if (g_roam_wp_x >  max_wx) g_roam_wp_x =  max_wx;
        if (g_roam_wp_y < -max_wy) g_roam_wp_y = -max_wy;
        if (g_roam_wp_y >  max_wy) g_roam_wp_y =  max_wy;
    }

    float move_vel = 0.0f;
    if (!aion_mouse_dragging && aion_drag_release_time == 0) {
        float target_pan_x = g_roam_wp_x - baseline_x;
        float target_pan_y = (g_roam_wp_y + 0.65f) - baseline_y;
        float diff_x = target_pan_x - aion_avatar_pan_x;
        float diff_y = target_pan_y - aion_avatar_pan_y;
        float dist = sqrtf(diff_x * diff_x + diff_y * diff_y);

        if (dist < 0.16f && g_roam_mode != 2) {
            // Reached autonomous destination -> switch to Paw-Pointing at the desktop feature!
            g_roam_mode = 2;
            g_point_paw_side = (g_roam_wp_x >= 0.0f) ? 1 : 0;
        }

        if (g_roam_mode != 2 && dist > 0.05f) {
            float max_step = (g_roam_mode == 1) ? 0.032f : 0.016f; // Run speed vs Walk speed
            float step = dist * ((g_roam_mode == 1) ? 0.065f : 0.035f);
            if (step > max_step) step = max_step;
            aion_avatar_pan_x += (diff_x / dist) * step;
            aion_avatar_pan_y += (diff_y / dist) * step;
            move_vel = step / 0.016f;

            // Turn body smoothly toward autonomous walking/running direction
            float walk_yaw = (diff_x / dist) * 38.0f;
            float walk_pitch = -(diff_y / dist) * 10.0f;
            aion_follow_rot_y += (walk_yaw - aion_follow_rot_y) * 0.08f;
            aion_follow_rot_x += (walk_pitch - aion_follow_rot_x) * 0.08f;
        } else {
            // Autonomous idle/speaking orientation (gentle natural look-around, decoupled from mouse!)
            float target_rot_y = sinf(anim_time * 0.65f) * 14.0f;
            float target_rot_x = cosf(anim_time * 0.45f) * 5.0f;
            aion_follow_rot_y += (target_rot_y - aion_follow_rot_y) * 0.05f;
            aion_follow_rot_x += (target_rot_x - aion_follow_rot_x) * 0.05f;
        }
    }

    // Scale GLB walk/run skeletal animation speed dynamically with movement velocity + audio
    float anim_speed = 0.45f + move_vel * 0.85f + (linux_current_freq * 0.015f);
    if (anim_speed > 2.4f) anim_speed = 2.4f;
    global_avatar.update(0.016f * anim_speed);

    // Procedural Bone Animations (Synchronous Speech Lip-Sync + Paw-Pointing + Tail/Ears)
    anim_time += 0.016f;
    
    if (jaw_idx == -2) {
        // 1. Check for Model 13 (Tuxedo Cat) lower jaw bone 'fst-Xtra01_16' (rotates around +Z to open mouth)
        jaw_idx = global_avatar.find_bone("fst-Xtra01");
        if (jaw_idx >= 0) {
            jaw_is_cat_z = 1;
        } else {
            jaw_is_cat_z = 0;
            jaw_idx = global_avatar.find_bone("Jaw");
            if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("jaw");
            if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("mixamorig:Jaw");
            if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("Head_");
            if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("Head");
        }
        tongue1_idx   = global_avatar.find_bone("tongue_bone01");
        tongue2_idx   = global_avatar.find_bone("tongue_bone02");
        whisker_l_idx = global_avatar.find_bone("Bone03_12");
        whisker_r_idx = global_avatar.find_bone("Bone02(mirrored)");
        ear_l_idx     = global_avatar.find_bone("cat_bip-Ponytail1");
        ear_r_idx     = global_avatar.find_bone("cat_bip-Ponytail2");
        head_bone_idx = global_avatar.find_bone("cat_bip-Head");
        if (head_bone_idx == -1) head_bone_idx = global_avatar.find_bone("Head");
    }
    if (spine_idx == -2) {
        spine_idx = global_avatar.find_bone("cat_bip-Spine1");
        if (spine_idx == -1) spine_idx = global_avatar.find_bone("Spine");
        if (spine_idx == -1) spine_idx = global_avatar.find_bone("spine");
        if (spine_idx == -1) spine_idx = global_avatar.find_bone("mixamorig:Spine");
        if (spine_idx == -1) spine_idx = global_avatar.find_bone("Chest");
    }
    if (arm_r_idx == -2) {
        arm_r_idx     = global_avatar.find_bone("cat_bip-R-UpperArm");
        forearm_r_idx = global_avatar.find_bone("cat_bip-R-Forearm");
        hand_r_idx    = global_avatar.find_bone("cat_bip-R-Hand");
        if (arm_r_idx == -1) arm_r_idx = global_avatar.find_bone("RightArm");
        if (arm_r_idx == -1) arm_r_idx = global_avatar.find_bone("mixamorig:RightArm");
    }
    if (arm_l_idx == -2) {
        arm_l_idx     = global_avatar.find_bone("cat_bip-L-UpperArm");
        forearm_l_idx = global_avatar.find_bone("cat_bip-L-Forearm");
        hand_l_idx    = global_avatar.find_bone("cat_bip-L-Hand");
        if (arm_l_idx == -1) arm_l_idx = global_avatar.find_bone("LeftArm");
        if (arm_l_idx == -1) arm_l_idx = global_avatar.find_bone("mixamorig:LeftArm");
    }
    if (tail0_idx == -2) {
        tail0_idx = global_avatar.find_bone("cat_bip-Tail_");
        tail1_idx = global_avatar.find_bone("cat_bip-Tail1");
        tail2_idx = global_avatar.find_bone("cat_bip-Tail2");
    }

    // 1. Breathing & Expressive Cat Tail Swish + Ear Twitches
    if (spine_idx >= 0) {
        float breath = sinf(anim_time * 2.2f) * 0.045f;
        global_avatar.add_procedural_rotation(spine_idx, 0.0f, 0.0f, 1.0f, breath);
    }
    if (tail0_idx >= 0) {
        float swish0 = sinf(anim_time * 2.6f) * 0.22f;
        global_avatar.add_procedural_rotation(tail0_idx, 0.0f, 1.0f, 0.0f, swish0);
    }
    if (tail1_idx >= 0) {
        float swish1 = sinf(anim_time * 2.6f - 0.6f) * 0.28f;
        global_avatar.add_procedural_rotation(tail1_idx, 0.0f, 1.0f, 0.0f, swish1);
    }
    if (tail2_idx >= 0) {
        float swish2 = cosf(anim_time * 2.6f - 1.1f) * 0.25f;
        global_avatar.add_procedural_rotation(tail2_idx, 0.0f, 1.0f, 0.0f, swish2);
    }
    if (ear_l_idx >= 0 && ear_r_idx >= 0) {
        float ear_perk = is_speaking ? (sinf(anim_time * 7.0f) * 0.12f) : (sinf(anim_time * 1.7f) * 0.05f);
        global_avatar.add_procedural_rotation(ear_l_idx, 0.0f, 0.0f, 1.0f, ear_perk);
        global_avatar.add_procedural_rotation(ear_r_idx, 0.0f, 0.0f, 1.0f, -ear_perk);
    }

    // 2. Synchronous Speech Lip-Sync (driven by real-time WAV audio envelope of /tmp/aion_response.wav)
    float mouth_open = compute_realtime_speech_envelope(is_speaking);
    if (jaw_idx >= 0 && mouth_open > 0.005f) {
        if (jaw_is_cat_z) {
            // Model 13 (Tuxedo Cat): rotating 'fst-Xtra01_16' around +Z opens the lower jaw & teeth downward!
            float jaw_angle = mouth_open * 0.44f;
            global_avatar.add_procedural_rotation(jaw_idx, 0.0f, 0.0f, 1.0f, jaw_angle);
            if (tongue1_idx >= 0) {
                global_avatar.add_procedural_rotation(tongue1_idx, 0.0f, 0.0f, 1.0f, -mouth_open * 0.18f);
            }
            if (tongue2_idx >= 0) {
                global_avatar.add_procedural_rotation(tongue2_idx, 0.0f, 0.0f, 1.0f, sinf(anim_time * 24.0f) * mouth_open * 0.15f);
            }
            if (whisker_l_idx >= 0) {
                global_avatar.add_procedural_rotation(whisker_l_idx, 0.0f, 0.0f, 1.0f, -mouth_open * 0.14f);
            }
            if (whisker_r_idx >= 0) {
                global_avatar.add_procedural_rotation(whisker_r_idx, 0.0f, 0.0f, 1.0f, -mouth_open * 0.14f);
            }
            if (head_bone_idx >= 0) {
                // Subtle natural speaking head nod synchronized with syllable emphasis
                global_avatar.add_procedural_rotation(head_bone_idx, 0.0f, 0.0f, 1.0f, mouth_open * 0.08f * sinf(anim_time * 9.0f));
            }
        } else {
            global_avatar.add_procedural_rotation(jaw_idx, 1.0f, 0.0f, 0.0f, mouth_open * 0.45f);
        }
    }

    // 3. Paw-Pointing & Expressive Front Paw Gestures (when pointing at desktop items or speaking)
    bool want_paw_point = (g_roam_mode == 2 || is_speaking) && (move_vel < 0.15f);
    float target_point = want_paw_point ? 1.0f : 0.0f;
    g_point_blend += (target_point - g_point_blend) * 0.10f;

    if (g_point_blend > 0.01f) {
        float point_tap = sinf(anim_time * 5.5f) * 0.14f;
        float lift_angle = -(0.62f + point_tap) * g_point_blend; // Negative Z lifts paw up and forward!
        float elbow_ext  = (0.35f + point_tap * 0.5f) * g_point_blend;
        float paw_flick  = sinf(anim_time * 8.0f) * 0.20f * g_point_blend;

        if (g_point_paw_side == 0 && arm_r_idx >= 0) {
            global_avatar.add_procedural_rotation(arm_r_idx, 0.0f, 0.0f, 1.0f, lift_angle);
            if (forearm_r_idx >= 0) global_avatar.add_procedural_rotation(forearm_r_idx, 0.0f, 0.0f, 1.0f, elbow_ext);
            if (hand_r_idx >= 0)    global_avatar.add_procedural_rotation(hand_r_idx, 0.0f, 0.0f, 1.0f, paw_flick);
        } else if (arm_l_idx >= 0) {
            global_avatar.add_procedural_rotation(arm_l_idx, 0.0f, 0.0f, 1.0f, lift_angle);
            if (forearm_l_idx >= 0) global_avatar.add_procedural_rotation(forearm_l_idx, 0.0f, 0.0f, 1.0f, elbow_ext);
            if (hand_l_idx >= 0)    global_avatar.add_procedural_rotation(hand_l_idx, 0.0f, 0.0f, 1.0f, paw_flick);
        }
    }
    
    global_avatar.recompute_global_matrices();
    global_avatar.skin();
    
    // Combine: manual rotation + mouse follow
    float final_rot_x = aion_avatar_rot_x + aion_follow_rot_x;
    float final_rot_y = aion_avatar_rot_y + aion_follow_rot_y;
    
    // SAVE entire OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    
    // 3D Projection Setup - NO scissor so model can extend beyond window
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    int draw_h = aion_window_h - 220 - 30;
    if (draw_h < 1) draw_h = 1;
    
    // FULLSCREEN VIEWPORT so she can walk anywhere
    glViewport(0, 0, global_screen_w, global_screen_h);
    
    glClear(GL_DEPTH_BUFFER_BIT); // Depth buffer clear for the whole screen
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float zNear = 0.1f;
    float zFar = 1000.0f;
    float fH = tanf(fov / 2.0f) * zNear;
    float fW = fH * aspect;
    
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Camera Transform (Anchored to AION window, plus free panning)
    // We adjust Y by -0.75f to center the model's feet roughly at the bottom of the virtual box
    glTranslatef(baseline_x + aion_avatar_pan_x, baseline_y - 0.75f + aion_avatar_pan_y, -3.0f * (1.0f / aion_avatar_zoom));
    
    // Apply combined rotations (manual + mouse follow)
    glRotatef(final_rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(final_rot_y, 0.0f, 1.0f, 0.0f);
    glRotatef(aion_avatar_rot_z, 0.0f, 0.0f, 1.0f);
    
    // Apply model's auto-centering and scaling (applied AFTER Z-to-Y fix)
    glScalef(global_avatar.auto_scale, global_avatar.auto_scale, global_avatar.auto_scale);
    glTranslatef(global_avatar.center_offset.x, global_avatar.center_offset.y, global_avatar.center_offset.z);
    
    // Fix Z-up to Y-up (applied FIRST to raw vertices)
    if (aion_current_avatar_idx == 8) {
        // Model 9 (index 8) needs extra rotation to stand up
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f); // Default
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f); // Extra 90
    } else {
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    }
    
    // Draw the GLB model
    global_avatar.draw();
    
    // RESTORE OpenGL state (pop saved state)
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    
    // EXPLICIT RESET of all critical state for safety
    glViewport(0, 0, global_screen_w, global_screen_h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

