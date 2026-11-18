#pragma once
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <mutex>
#include <deque>
#include <SDL2/SDL.h>

extern "C" uint32_t aion_window_x;
extern "C" uint32_t aion_window_y;
extern "C" uint32_t aion_window_w;
extern "C" uint32_t aion_window_h;
extern "C" uint8_t  aion_window_open;
extern "C" uint8_t  aion_use_3d_avatar;
extern uint32_t linux_current_freq;

float aion_avatar_zoom  = 0.72f;
float aion_avatar_rot_x = 8.0f;
float aion_avatar_rot_y = -24.0f;
float aion_avatar_rot_z = 0.0f;
float aion_avatar_pan_x = 0.0f;
float aion_avatar_pan_y = 0.0f;
int aion_avatar_screen_x = 0;
int aion_avatar_screen_y = 0;
bool aion_mouse_dragging = false;

static int jaw_idx = -2;
static int jaw_is_cat_z = 0;
static int jaw_is_rot_x_neg = 0;
static int tongue1_idx = -2;
static int tongue2_idx = -2;
static int whisker_l_idx = -2;
static int whisker_r_idx = -2;
static int ear_l_idx = -2;
static int ear_r_idx = -2;
static int head_bone_idx = -2;
static int pelvis_idx = -2;
static int spine_idx = -2;
static int arm_r_idx = -2;
static int forearm_r_idx = -2;
static int hand_r_idx = -2;
static int arm_l_idx = -2;
static int forearm_l_idx = -2;
static int hand_l_idx = -2;
static int leg_l_idx = -2;
static int leg_r_idx = -2;
static int tail0_idx = -2;
static int tail1_idx = -2;
static int tail2_idx = -2;
static float anim_time = 0.0f;

#include "avatar_anim.h"

SkeletalAvatar global_avatar;

extern "C" uint8_t  aion_switch_avatar = 0;
extern "C" int      aion_current_avatar_idx = 12; // Model 13/13 (Tuxedo Cat) as default main character
extern "C" int      aion_total_models = 0;

// --- Per-Model Metadata (calibrated for true glTF +Y up world space) ---
struct AvatarModelMeta {
    const char* name;
    const char* category;
    float default_yaw;
    float default_pitch;
    float default_zoom;
    bool is_quadruped;
    bool is_mixamo_biped;
};

static const int kNumKnownModels = 13;
static const AvatarModelMeta kAvatarModels[kNumKnownModels] = {
    { "Stylized Companion 01", "stylized", -20.0f, 6.0f, 0.74f, false, false }, // 1: model_0
    { "Stylized Companion 02", "stylized", -20.0f, 6.0f, 0.74f, false, false }, // 2: model_1
    { "Sporty Girl",           "human",    -18.0f, 5.0f, 0.70f, false, false }, // 3: model_2
    { "Cyber Figure 04",       "stylized", -18.0f, 6.0f, 0.74f, false, false }, // 4: model_3
    { "Humanoid Avatar 05",    "human",    -18.0f, 5.0f, 0.70f, false, false }, // 5: model_4
    { "Girl in Dress",         "human",    -18.0f, 5.0f, 0.68f, false, true  }, // 6: model_5
    { "Stylized Boy",          "human",    -20.0f, 5.0f, 0.72f, false, false }, // 7: model_6
    { "Animated Woman",        "human",    -18.0f, 5.0f, 0.68f, false, true  }, // 8: model_7
    { "Animated Cat",          "cat",      -68.0f, 7.0f, 0.72f, true,  false }, // 9: model_8 (+X forward)
    { "Realistic Lioness",     "lioness",  -26.0f, 7.0f, 0.74f, true,  false }, // 10: model_9 (3 clips)
    { "Animated Lion",         "lion",     -25.0f, 7.0f, 0.70f, true,  false }, // 11: model_10
    { "Sleek Cat",             "cat",      -24.0f, 7.0f, 0.74f, true,  false }, // 12: model_11
    { "Tuxedo Cat",            "cat",      -24.0f, 7.0f, 0.74f, true,  false }  // 13: model_12 (default)
};

static const AvatarModelMeta& get_model_meta(int idx) {
    if (idx < 0 || idx >= kNumKnownModels) return kAvatarModels[12];
    return kAvatarModels[idx];
}

extern "C" const char* AionGetAvatarModelName(int idx) {
    return get_model_meta(idx).name;
}

// --- Action & Embodiment State Machine ---
enum AvatarActionState {
    AV_IDLE = 0,
    AV_WALK,
    AV_COME,
    AV_DANCE,
    AV_WAVE,
    AV_SPIN,
    AV_JUMP,
    AV_SIT,
    AV_POINT
};

static AvatarActionState g_action_state = AV_IDLE;
static float g_action_timer = 0.0f;
static float g_action_elapsed = 0.0f;
static float g_idle_patrol_cooldown = 16.0f;

// Sticky continuous dance & background music groove state
static bool  g_dance_persistent = false;
static float g_dance_endurance_timer = 0.0f;
static float g_dance_phase = 0.0f;
static float g_overlay_wave_timer = 0.0f;
static bool  g_music_dancing = false;
static float g_music_level = 0.0f;
static float g_music_active_hold = 0.0f;
static float g_music_quiet_hold = 0.0f;

// Normalized desktop coordinates (sx in [-1..1], sy in [-1..1], ground near -0.55)
static float g_pos_sx = 0.42f;
static float g_pos_sy = -0.38f;
static float g_target_sx = 0.42f;
static float g_target_sy = -0.38f;
static bool  g_pos_initialized = false;

static float g_body_yaw_offset = 0.0f;
static float g_body_pitch_offset = 0.0f;
static float g_body_roll_offset = 0.0f;

static float g_wave_blend = 0.0f;
static float g_dance_blend = 0.0f;
static float g_sit_blend = 0.0f;
static float g_point_blend = 0.0f;
static int   g_point_side = 0;

static std::mutex g_cmd_mutex;
static std::deque<std::string> g_cmd_queue;
static char g_status_buf[256] = "Model 13/13 (Tuxedo Cat) | State: IDLE";
static std::mutex g_status_mutex;

static const char* action_state_name(AvatarActionState st) {
    if (g_dance_persistent) return "DANCING (continuous)";
    if (g_music_dancing && st == AV_IDLE) return "CHILL_DANCING_TO_MUSIC";
    switch (st) {
        case AV_WALK:  return "WALKING";
        case AV_COME:  return "COMING_TO_USER";
        case AV_DANCE: return "DANCING (continuous)";
        case AV_WAVE:  return "WAVING";
        case AV_SPIN:  return "SPINNING";
        case AV_JUMP:  return "JUMPING";
        case AV_SIT:   return "SITTING";
        case AV_POINT: return "POINTING";
        default:       return "IDLE";
    }
}

extern "C" void AionTriggerAvatarAction(const char* cmd) {
    if (!cmd || !cmd[0]) return;
    std::lock_guard<std::mutex> lock(g_cmd_mutex);
    g_cmd_queue.emplace_back(cmd);
}

extern "C" const char* AionGetAvatarStatusText() {
    std::lock_guard<std::mutex> lock(g_status_mutex);
    return g_status_buf;
}

extern "C" bool AionIsAvatarDancing() {
    return g_dance_persistent || g_action_state == AV_DANCE;
}

// --- Config V2 (avoids legacy corrupted -90deg axis .bin files) ---
struct AvatarConfigV2 {
    uint32_t magic; // 0x41563032 ("AV02")
    float rot_x;
    float rot_y;
    float rot_z;
    float zoom;
};

static void apply_model_default_pose(int idx) {
    const AvatarModelMeta& m = get_model_meta(idx);
    aion_avatar_rot_x = m.default_pitch;
    aion_avatar_rot_y = m.default_yaw;
    aion_avatar_rot_z = 0.0f;
    aion_avatar_zoom  = m.default_zoom;
    aion_avatar_pan_x = 0.0f;
    aion_avatar_pan_y = 0.0f;
}

extern "C" void aion_save_config(int idx) {
    char path[256];
    snprintf(path, sizeof(path), "/opt/meinos/avatar_config_v2_%d.bin", idx);
    FILE* f = fopen(path, "wb");
    if (f) {
        AvatarConfigV2 cfg{ 0x41563032U, aion_avatar_rot_x, aion_avatar_rot_y, aion_avatar_rot_z, aion_avatar_zoom };
        fwrite(&cfg, sizeof(cfg), 1, f);
        fclose(f);
    }
}

extern "C" void aion_load_config(int idx) {
    apply_model_default_pose(idx);
    char path[256];
    snprintf(path, sizeof(path), "/opt/meinos/avatar_config_v2_%d.bin", idx);
    FILE* f = fopen(path, "rb");
    if (f) {
        AvatarConfigV2 cfg{};
        if (fread(&cfg, sizeof(cfg), 1, f) == 1 && cfg.magic == 0x41563032U) {
            if (cfg.zoom >= 0.20f && cfg.zoom <= 5.0f && fabsf(cfg.rot_x) <= 60.0f) {
                aion_avatar_rot_x = cfg.rot_x;
                aion_avatar_rot_y = cfg.rot_y;
                aion_avatar_rot_z = cfg.rot_z;
                aion_avatar_zoom  = cfg.zoom;
            }
        }
        fclose(f);
    }
}

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
        size_t window_samples = g_speech_sample_rate / 30;
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
            float env = (rms * 3.8f + pk * 0.9f);
            if (env < 0.04f) env = 0.0f;
            else env = (env - 0.04f) / 0.55f;
            if (env > 1.0f) env = 1.0f;
            float micro = 0.82f + 0.18f * sinf(anim_time * 28.0f);
            target_open = env * micro;
        } else {
            float cad = sinf(anim_time * 16.0f) * 0.5f + sinf(anim_time * 25.0f) * 0.35f + 0.25f;
            target_open = (cad > 0.0f) ? cad : 0.0f;
        }
    } else if (is_speaking) {
        float cad = sinf(anim_time * 16.0f) * 0.5f + sinf(anim_time * 25.0f) * 0.35f + 0.25f;
        target_open = (cad > 0.0f) ? cad : 0.0f;
    }

    float rate = (target_open > g_smooth_mouth_open) ? 0.52f : 0.28f;
    g_smooth_mouth_open += (target_open - g_smooth_mouth_open) * rate;
    if (g_smooth_mouth_open < 0.005f) g_smooth_mouth_open = 0.0f;
    return g_smooth_mouth_open;
}

static void reset_bone_cache() {
    jaw_idx = -2;
    jaw_is_cat_z = 0;
    jaw_is_rot_x_neg = 0;
    tongue1_idx = -2;
    tongue2_idx = -2;
    whisker_l_idx = -2;
    whisker_r_idx = -2;
    ear_l_idx = -2;
    ear_r_idx = -2;
    head_bone_idx = -2;
    pelvis_idx = -2;
    spine_idx = -2;
    arm_r_idx = -2;
    forearm_r_idx = -2;
    hand_r_idx = -2;
    arm_l_idx = -2;
    forearm_l_idx = -2;
    hand_l_idx = -2;
    leg_l_idx = -2;
    leg_r_idx = -2;
    tail0_idx = -2;
    tail1_idx = -2;
    tail2_idx = -2;
}

static int find_first_bone(const char* const* candidates, int count) {
    for (int i = 0; i < count; ++i) {
        int b = global_avatar.find_bone(candidates[i]);
        if (b >= 0) return b;
    }
    return -1;
}

static void bind_avatar_bones() {
    if (jaw_idx != -2) return;

    int cat_jaw = global_avatar.find_bone("fst-Xtra01");
    if (cat_jaw >= 0) {
        jaw_idx = cat_jaw;
        jaw_is_cat_z = 1;
    } else {
        jaw_is_cat_z = 0;
        const char* jaws[] = { "j_jaw", "Jaw", "jaw", "mixamorig:Jaw", "Head_", "Head_0", "mixamorig:Head" };
        jaw_idx = find_first_bone(jaws, 7);
        jaw_is_rot_x_neg = (jaw_idx >= 0 && (global_avatar.find_bone("j_jaw") >= 0 || global_avatar.find_bone("Jaw") >= 0));
    }

    const char* t1[] = { "tongue_bone01", "Tongue1", "j_tongue_back" };
    const char* t2[] = { "tongue_bone02", "Tongue2", "j_tongue_mid" };
    tongue1_idx = find_first_bone(t1, 3);
    tongue2_idx = find_first_bone(t2, 3);
    whisker_l_idx = global_avatar.find_bone("Bone03_12");
    whisker_r_idx = global_avatar.find_bone("Bone02(mirrored)");

    const char* el[] = { "cat_bip-Ponytail1", "EarL1", "Ear1.L", "j_l_ear" };
    const char* er[] = { "cat_bip-Ponytail2", "EarR1", "Ear1.R", "j_r_ear" };
    ear_l_idx = find_first_bone(el, 4);
    ear_r_idx = find_first_bone(er, 4);

    const char* hd[] = { "cat_bip-Head", "j_head", "Head_0", "Head_", "mixamorig:Head", "Head" };
    head_bone_idx = find_first_bone(hd, 6);

    const char* pv[] = { "cat_bip-Pelvis", "Pelvis_", "Body_36", "j_body_00", "mixamorig:Hips" };
    pelvis_idx = find_first_bone(pv, 5);

    const char* sp[] = { "cat_bip-Spine1", "Spine1_", "Torso_", "j_spine_2", "mixamorig:Spine1", "mixamorig:Spine", "Spine", "Chest" };
    spine_idx = find_first_bone(sp, 8);

    const char* ar[] = { "cat_bip-R-UpperArm", "mixamorig:RightArm", "j_r_humerous", "LegFR1", "FrontUpperLeg.R", "RightArm" };
    const char* fr[] = { "cat_bip-R-Forearm", "mixamorig:RightForeArm", "j_r_forearm", "LegFR2", "FrontLowerLeg.R" };
    const char* hr[] = { "cat_bip-R-Hand", "mixamorig:RightHand", "j_r_wrist", "LegFR3" };
    arm_r_idx = find_first_bone(ar, 6);
    forearm_r_idx = find_first_bone(fr, 5);
    hand_r_idx = find_first_bone(hr, 4);

    const char* al[] = { "cat_bip-L-UpperArm", "mixamorig:LeftArm", "j_l_humerous", "LegFL1", "FrontUpperLeg.L", "LeftArm" };
    const char* fl[] = { "cat_bip-L-Forearm", "mixamorig:LeftForeArm", "j_l_forearm", "LegFL2", "FrontLowerLeg.L" };
    const char* hl[] = { "cat_bip-L-Hand", "mixamorig:LeftHand", "j_l_wrist", "LegFL3" };
    arm_l_idx = find_first_bone(al, 6);
    forearm_l_idx = find_first_bone(fl, 5);
    hand_l_idx = find_first_bone(hl, 4);

    const char* ll[] = { "cat_bip-L-Thigh", "mixamorig:LeftUpLeg", "LegBL1", "BackUpperLeg.L" };
    const char* lr[] = { "cat_bip-R-Thigh", "mixamorig:RightUpLeg", "LegBR1", "BackUpperLeg.R" };
    leg_l_idx = find_first_bone(ll, 4);
    leg_r_idx = find_first_bone(lr, 4);

    const char* ta0[] = { "cat_bip-Tail_", "Tail1_", "j_tail_1" };
    const char* ta1[] = { "cat_bip-Tail1", "Tail2_", "j_tail_2" };
    const char* ta2[] = { "cat_bip-Tail2", "Tail3_", "j_tail_3" };
    tail0_idx = find_first_bone(ta0, 3);
    tail1_idx = find_first_bone(ta1, 3);
    tail2_idx = find_first_bone(ta2, 3);
}

void load_a3d(const char* path) {
    global_avatar.load_glb(path);
    reset_bone_cache();
    if (global_avatar.is_loaded()) {
        printf("Successfully loaded GLB model %s (clips=%d, bones=%d)\n",
               path, global_avatar.get_animation_count(), global_avatar.get_bone_count());
    } else {
        printf("Failed to load GLB model %s\n", path);
    }
}

static void get_home_screen_anchor(float& out_sx, float& out_sy) {
    extern int global_screen_w;
    extern int global_screen_h;
    if (aion_window_open && aion_window_w > 0 && global_screen_w > 0 && global_screen_h > 0) {
        // Cozy companion perch beside the AION NEXUS window (or bottom-right floor if window fills right)
        float win_right_px = (float)(aion_window_x + aion_window_w);
        float px = (win_right_px + 220.0f < (float)global_screen_w)
            ? (win_right_px + 135.0f)
            : ((aion_window_x > 260.0f) ? ((float)aion_window_x - 135.0f) : ((float)global_screen_w * 0.72f));
        float py = (float)(aion_window_y + aion_window_h) - 110.0f;
        if (py > (float)global_screen_h - 140.0f) py = (float)global_screen_h - 140.0f;
        if (py < 240.0f) py = 240.0f;
        out_sx = (px / (float)global_screen_w) * 2.0f - 1.0f;
        out_sy = 1.0f - (py / (float)global_screen_h) * 2.0f;
    } else {
        out_sx = 0.44f;
        out_sy = -0.46f;
    }
    if (out_sx < -0.78f) out_sx = -0.78f;
    if (out_sx >  0.78f) out_sx =  0.78f;
    if (out_sy < -0.72f) out_sy = -0.72f;
    if (out_sy >  0.42f) out_sy =  0.42f;
}

static void execute_avatar_command_str(const std::string& raw_cmd) {
    std::string cmd = raw_cmd;
    // Trim & uppercase copy for matching
    size_t s = cmd.find_first_not_of(" \t\r\n");
    if (s == std::string::npos) return;
    size_t e = cmd.find_last_not_of(" \t\r\n");
    cmd = cmd.substr(s, e - s + 1);

    std::string up = cmd;
    for (char& ch : up) ch = (char)toupper((unsigned char)ch);

    if (up.find("MODEL") != std::string::npos) {
        // Parse 1-based model index 1..aion_total_models (or 1..13)
        int num = -1;
        for (size_t i = 0; i < up.size(); ++i) {
            if (isdigit((unsigned char)up[i])) {
                num = atoi(up.c_str() + i);
                break;
            }
        }
        int limit = (aion_total_models > 0) ? aion_total_models : kNumKnownModels;
        if (up.find("NEXT") != std::string::npos) {
            aion_save_config(aion_current_avatar_idx);
            aion_current_avatar_idx = (aion_current_avatar_idx + 1) % limit;
            aion_switch_avatar = 1;
        } else if (up.find("PREV") != std::string::npos) {
            aion_save_config(aion_current_avatar_idx);
            aion_current_avatar_idx = (aion_current_avatar_idx - 1 + limit) % limit;
            aion_switch_avatar = 1;
        } else if (num >= 1 && num <= limit) {
            if (num - 1 != aion_current_avatar_idx) {
                aion_save_config(aion_current_avatar_idx);
                aion_current_avatar_idx = num - 1;
                aion_switch_avatar = 1;
            }
        } else {
            // Try matching by category keyword (e.g. LIONESS, LION, GIRL, WOMAN, BOY, TUXEDO)
            if (up.find("LIONESS") != std::string::npos && limit >= 10) {
                aion_current_avatar_idx = 9; aion_switch_avatar = 1;
            } else if (up.find("LION") != std::string::npos && limit >= 11) {
                aion_current_avatar_idx = 10; aion_switch_avatar = 1;
            } else if ((up.find("GIRL") != std::string::npos || up.find("DRESS") != std::string::npos) && limit >= 6) {
                aion_current_avatar_idx = 5; aion_switch_avatar = 1;
            } else if ((up.find("WOMAN") != std::string::npos || up.find("MIXAMO") != std::string::npos) && limit >= 8) {
                aion_current_avatar_idx = 7; aion_switch_avatar = 1;
            } else if (up.find("BOY") != std::string::npos && limit >= 7) {
                aion_current_avatar_idx = 6; aion_switch_avatar = 1;
            } else if ((up.find("TUXEDO") != std::string::npos || up.find("DEFAULT") != std::string::npos) && limit >= 13) {
                aion_current_avatar_idx = 12; aion_switch_avatar = 1;
            }
        }
        return;
    }

    if (up.find("RESET") != std::string::npos) {
        apply_model_default_pose(aion_current_avatar_idx);
        aion_save_config(aion_current_avatar_idx);
        get_home_screen_anchor(g_target_sx, g_target_sy);
        g_dance_persistent = false;
        g_dance_endurance_timer = 0.0f;
        g_action_state = AV_IDLE;
        g_action_timer = 0.0f;
        g_action_elapsed = 0.0f;
        return;
    }

    if (up.find("STOP") != std::string::npos || up.find("AUFHOER") != std::string::npos ||
        up.find("PAUSE") != std::string::npos || up.find("IDLE") != std::string::npos ||
        up.find("CALM") != std::string::npos || up.find("STAND") != std::string::npos ||
        up.find("TIRED") != std::string::npos || up.find("HALT") != std::string::npos) {
        g_dance_persistent = false;
        g_dance_endurance_timer = 0.0f;
        g_action_state = AV_IDLE;
        g_action_timer = 0.0f;
        g_action_elapsed = 0.0f;
        return;
    }

    if (up.find("DANCE") != std::string::npos || up.find("TANZ") != std::string::npos || up.find("GROOVE") != std::string::npos) {
        g_dance_persistent = true;
        g_dance_endurance_timer = 150.0f + (float)(rand() % 60); // 2.5 - 3.5 min unless stopped earlier
        g_action_state = AV_DANCE;
        g_action_timer = g_dance_endurance_timer;
        g_action_elapsed = 0.0f;
        aion_use_3d_avatar = 1;
    } else if (up.find("WAVE") != std::string::npos || up.find("GREET") != std::string::npos || up.find("WINK") != std::string::npos) {
        if (g_dance_persistent) {
            // Brief friendly wave without interrupting continuous dance!
            g_overlay_wave_timer = 2.6f;
        } else {
            g_action_state = AV_WAVE;
            g_action_timer = 3.8f;
            g_action_elapsed = 0.0f;
        }
    } else if (up.find("SPIN") != std::string::npos || up.find("TURN") != std::string::npos || up.find("TWIRL") != std::string::npos) {
        g_action_state = AV_SPIN;
        g_action_timer = 2.2f;
        g_action_elapsed = 0.0f;
    } else if (up.find("JUMP") != std::string::npos || up.find("HOP") != std::string::npos || up.find("LEAP") != std::string::npos) {
        g_action_state = AV_JUMP;
        g_action_timer = 1.6f;
        g_action_elapsed = 0.0f;
    } else if (up.find("SIT") != std::string::npos || up.find("REST") != std::string::npos || up.find("SETZ") != std::string::npos) {
        g_dance_persistent = false;
        g_dance_endurance_timer = 0.0f;
        g_action_state = AV_SIT;
        g_action_timer = 14.0f;
        g_action_elapsed = 0.0f;
    } else if (up.find("COME") != std::string::npos || up.find("HERE") != std::string::npos) {
        g_dance_persistent = false;
        g_dance_endurance_timer = 0.0f;
        get_home_screen_anchor(g_target_sx, g_target_sy);
        g_action_state = AV_COME;
        g_action_timer = 5.0f;
        g_action_elapsed = 0.0f;
    } else if (up.find("WALK") != std::string::npos || up.find("PATROL") != std::string::npos || up.find("STROLL") != std::string::npos) {
        g_dance_persistent = false;
        g_dance_endurance_timer = 0.0f;
        static const float lane_x[5] = { -0.52f, -0.22f, 0.08f, 0.38f, 0.62f };
        int pick = rand() % 5;
        g_target_sx = lane_x[pick];
        g_target_sy = -0.46f + ((rand() % 100) / 100.0f) * 0.14f;
        g_action_state = AV_WALK;
        g_action_timer = 6.5f;
        g_action_elapsed = 0.0f;
    } else if (up.find("POINT") != std::string::npos) {
        if (!g_dance_persistent) {
            g_action_state = AV_POINT;
            g_action_timer = 4.0f;
            g_action_elapsed = 0.0f;
            g_point_side = (g_pos_sx > 0.0f) ? 1 : 0;
        }
    }
}

static void poll_avatar_commands() {
    {
        std::lock_guard<std::mutex> lock(g_cmd_mutex);
        while (!g_cmd_queue.empty()) {
            execute_avatar_command_str(g_cmd_queue.front());
            g_cmd_queue.pop_front();
        }
    }

    static int poll_counter = 0;
    if ((poll_counter++ % 6) == 0) {
        FILE* cf = fopen("/tmp/aion_avatar_cmd.txt", "r");
        if (cf) {
            char buf[256] = {0};
            if (fgets(buf, sizeof(buf) - 1, cf)) {
                fclose(cf);
                remove("/tmp/aion_avatar_cmd.txt");
                // Allow semicolon-separated commands e.g. "MODEL 10; DANCE"
                char* savep = nullptr;
                char* tok = strtok_r(buf, ";\n", &savep);
                while (tok) {
                    execute_avatar_command_str(tok);
                    tok = strtok_r(nullptr, ";\n", &savep);
                }
            } else {
                fclose(cf);
                remove("/tmp/aion_avatar_cmd.txt");
            }
        }
    }
}

void draw_a3d() {
    poll_avatar_commands();

    if (aion_switch_avatar) {
        aion_switch_avatar = 0;
        char path[256];
        snprintf(path, sizeof(path), "/opt/meinos/model_%d.glb", aion_current_avatar_idx);
        load_a3d(path);
        aion_load_config(aion_current_avatar_idx);
    }

    if (!global_avatar.is_loaded() || !aion_use_3d_avatar) return;

    extern int global_screen_w;
    extern int global_screen_h;
    if (global_screen_w <= 0 || global_screen_h <= 0) return;

    const float dt = 0.016f;
    anim_time += dt;
    aion_mouse_dragging = false;

    const AvatarModelMeta& meta = get_model_meta(aion_current_avatar_idx);
    if (!g_pos_initialized) {
        get_home_screen_anchor(g_pos_sx, g_pos_sy);
        g_target_sx = g_pos_sx;
        g_target_sy = g_pos_sy;
        g_pos_initialized = true;
    }

    // Check speaking flag
    FILE* sf = fopen("/tmp/aion_speaking", "r");
    bool is_speaking = false;
    if (sf) {
        is_speaking = true;
        fclose(sf);
    }

    // Background music / radio beat detection (ignores AION's own TTS speech)
    static bool s_radio_socket = false;
    static int s_music_stat_div = 0;
    if ((s_music_stat_div++ % 30) == 0) {
        struct stat st_mpv;
        s_radio_socket = (stat("/tmp/mpvsocket", &st_mpv) == 0);
    }
    float raw_music = s_radio_socket ? 0.42f : 0.0f;
    if (!is_speaking && linux_current_freq > 28) {
        float amp_lvl = (float)(linux_current_freq - 28) / 140.0f;
        if (amp_lvl > 1.0f) amp_lvl = 1.0f;
        if (amp_lvl > raw_music) raw_music = amp_lvl;
    }
    g_music_level += (raw_music - g_music_level) * 0.08f;

    if (s_radio_socket || g_music_level > 0.22f) {
        g_music_active_hold += dt;
        g_music_quiet_hold = 0.0f;
        if (g_music_active_hold > 0.65f) g_music_dancing = true;
    } else if (g_music_level < 0.09f) {
        g_music_quiet_hold += dt;
        g_music_active_hold = 0.0f;
        if (g_music_quiet_hold > 4.0f) g_music_dancing = false;
    }

    // Speaking ONLY pauses walking/coming — NEVER aborts dancing!
    if (is_speaking && (g_action_state == AV_WALK || g_action_state == AV_COME)) {
        g_action_state = AV_IDLE;
        g_action_timer = 0.0f;
    }

    if (g_overlay_wave_timer > 0.0f) {
        g_overlay_wave_timer -= dt;
        if (g_overlay_wave_timer < 0.0f) g_overlay_wave_timer = 0.0f;
    }

    // Endurance timer for persistent explicit dance (2.5 - 3.5 min until she wants a break)
    if (g_dance_persistent) {
        g_dance_endurance_timer -= dt;
        if (g_dance_endurance_timer <= 0.0f) {
            g_dance_persistent = false;
            if (g_action_state == AV_DANCE) g_action_state = AV_IDLE;
        }
    }

    // Timed action countdown
    if (g_action_state != AV_IDLE) {
        g_action_elapsed += dt;
        g_action_timer -= dt;
        if (g_action_timer <= 0.0f) {
            g_action_state = g_dance_persistent ? AV_DANCE : AV_IDLE;
            g_action_timer = g_dance_persistent ? g_dance_endurance_timer : 0.0f;
            g_action_elapsed = 0.0f;
        }
    } else if (!g_dance_persistent && !g_music_dancing) {
        // Calm occasional ambient behavior ONLY when truly idle & no music playing
        g_idle_patrol_cooldown -= dt;
        if (g_idle_patrol_cooldown <= 0.0f && !is_speaking) {
            g_idle_patrol_cooldown = 16.0f + (float)(rand() % 12);
            float hx = 0.42f, hy = -0.38f;
            get_home_screen_anchor(hx, hy);
            int choice = rand() % 100;
            if (choice < 40) {
                g_target_sx = hx + (((rand() % 100) / 100.0f) - 0.5f) * 0.30f;
                g_target_sy = hy + (((rand() % 100) / 100.0f) - 0.5f) * 0.08f;
                if (g_target_sx < -0.70f) g_target_sx = -0.70f;
                if (g_target_sx >  0.70f) g_target_sx =  0.70f;
                if (g_target_sy < -0.68f) g_target_sy = -0.68f;
                if (g_target_sy >  0.25f) g_target_sy =  0.25f;
                g_action_state = AV_WALK;
                g_action_timer = 4.2f;
                g_action_elapsed = 0.0f;
            } else if (choice < 75) {
                g_action_state = AV_SIT;
                g_action_timer = 9.0f;
                g_action_elapsed = 0.0f;
            } else {
                g_action_state = AV_WAVE;
                g_action_timer = 2.8f;
                g_action_elapsed = 0.0f;
            }
        }
    }

    // Determine whether she is actively dancing (explicit persistent request OR calm music groove)
    bool explicit_dance_active = g_dance_persistent && (g_action_state == AV_DANCE || g_action_state == AV_IDLE);
    bool music_groove_active = (!g_dance_persistent && g_music_dancing && g_action_state == AV_IDLE);
    bool dance_active_now = explicit_dance_active || music_groove_active;

    if (dance_active_now) {
        float tempo = explicit_dance_active ? (3.4f + g_music_level * 0.9f) : (2.75f + g_music_level * 0.8f);
        g_dance_phase += dt * tempo;
    } else {
        g_dance_phase += dt * 2.5f;
    }

    // Soften body amplitude slightly when talking so lip-sync stays clear while continuing to dance!
    float base_dance_target = explicit_dance_active ? 0.92f : (music_groove_active ? 0.52f : 0.0f);
    if (is_speaking && base_dance_target > 0.0f) base_dance_target *= 0.68f;
    g_dance_blend += (base_dance_target - g_dance_blend) * 0.08f;

    // --- Locomotion in Normalized Screen Plane ---
    float move_speed = 0.0f;
    float target_body_yaw = 0.0f;
    float target_body_pitch = 0.0f;
    float target_body_roll = 0.0f;
    float extra_lift_y = 0.0f;
    float extra_shift_x = 0.0f;
    float scale_mod_y = 1.0f;
    float scale_mod_xz = 1.0f;

    if (g_action_state == AV_WALK || g_action_state == AV_COME) {
        float dx = g_target_sx - g_pos_sx;
        float dy = g_target_sy - g_pos_sy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.015f) {
            float speed = (g_action_state == AV_COME) ? 0.28f : 0.16f;
            float step = fminf(dist, speed * dt);
            g_pos_sx += (dx / dist) * step;
            g_pos_sy += (dy / dist) * step;
            move_speed = step / dt;
            target_body_yaw = (dx >= 0.0f) ? 62.0f : -62.0f;
        } else {
            g_action_state = AV_IDLE;
            g_action_timer = 0.0f;
        }
    } else if (dance_active_now || g_dance_blend > 0.05f) {
        float d = g_dance_blend;
        float ph = g_dance_phase;
        // Smooth musical sway & cushion bounce (calm, rhythmic, not hectic!)
        extra_lift_y  = (0.5f - 0.5f * cosf(ph * 2.0f)) * 0.055f * d;
        extra_shift_x = sinf(ph) * 0.095f * d;
        target_body_roll = sinf(ph) * 6.5f * d;
        if (explicit_dance_active && !is_speaking) {
            float spin_cycle = fmodf(g_action_elapsed, 13.0f);
            if (spin_cycle > 11.2f) {
                float u = (spin_cycle - 11.2f) / 1.8f;
                float smooth = u * u * (3.0f - 2.0f * u);
                target_body_yaw = smooth * 360.0f;
            } else {
                target_body_yaw = sinf(ph) * 19.0f * d;
            }
        } else {
            target_body_yaw = sinf(ph) * 14.0f * d;
        }
        scale_mod_y  = 1.0f + sinf(ph * 2.0f) * 0.022f * d;
        scale_mod_xz = 1.0f - sinf(ph * 2.0f) * 0.014f * d;
    } else if (g_action_state == AV_SPIN) {
        float p = g_action_elapsed / 2.2f;
        if (p > 1.0f) p = 1.0f;
        float smooth = p * p * (3.0f - 2.0f * p);
        target_body_yaw = smooth * 360.0f;
        extra_lift_y = sinf(3.14159f * p) * 0.22f;
    } else if (g_action_state == AV_JUMP) {
        float p = g_action_elapsed / 1.6f;
        if (p > 1.0f) p = 1.0f;
        if (p < 0.18f) {
            float c = p / 0.18f;
            scale_mod_y = 1.0f - 0.13f * sinf(c * 1.5708f);
            scale_mod_xz = 1.0f + 0.08f * sinf(c * 1.5708f);
        } else if (p < 0.78f) {
            float a = (p - 0.18f) / 0.60f;
            extra_lift_y = 4.0f * 0.52f * a * (1.0f - a);
            scale_mod_y = 1.06f;
            scale_mod_xz = 0.96f;
        } else {
            float l = (p - 0.78f) / 0.22f;
            float sq = sinf(l * 3.14159f) * 0.11f;
            scale_mod_y = 1.0f - sq;
            scale_mod_xz = 1.0f + sq * 0.6f;
        }
    } else if (g_action_state == AV_SIT) {
        extra_lift_y = -0.10f * g_sit_blend;
        target_body_pitch = -8.0f * g_sit_blend;
    } else if (g_action_state == AV_WAVE) {
        target_body_roll = 5.0f;
        target_body_yaw = sinf(g_action_elapsed * 2.5f) * 10.0f;
    } else if (g_action_state == AV_POINT) {
        target_body_yaw = (g_point_side == 0) ? -22.0f : 22.0f;
    } else {
        target_body_yaw = sinf(anim_time * 0.75f) * 10.0f;
        target_body_pitch = cosf(anim_time * 0.55f) * 3.0f;
    }

    g_body_yaw_offset   += (target_body_yaw   - g_body_yaw_offset)   * 0.12f;
    g_body_pitch_offset += (target_body_pitch - g_body_pitch_offset) * 0.12f;
    g_body_roll_offset  += (target_body_roll  - g_body_roll_offset)  * 0.12f;

    bool wave_wanted = (g_action_state == AV_WAVE || g_overlay_wave_timer > 0.0f);
    g_wave_blend  += ((wave_wanted ? 1.0f : 0.0f) - g_wave_blend) * 0.12f;
    g_sit_blend   += (((g_action_state == AV_SIT) ? 1.0f : 0.0f) - g_sit_blend) * 0.10f;
    bool want_point = (g_action_state == AV_POINT) || (is_speaking && move_speed < 0.02f && !dance_active_now && g_action_state == AV_IDLE);
    g_point_blend += ((want_point ? 1.0f : 0.0f) - g_point_blend) * 0.10f;

    // --- Animation Clip Selection & Rest Stance Control (NO mid-air walking when idle!) ---
    int clip_count = global_avatar.get_animation_count();
    float clip_dt = 0.0f;
    if (clip_count > 1) {
        if (move_speed > 0.02f) {
            int wc = global_avatar.find_animation_clip("walk");
            if (wc < 0) wc = global_avatar.find_animation_clip("run");
            if (wc >= 0) global_avatar.set_animation_clip(wc, false);
            clip_dt = dt * (0.75f + move_speed * 2.2f);
        } else {
            int ic = global_avatar.find_animation_clip("idle");
            if (ic < 0) ic = global_avatar.find_animation_clip("breath");
            if (ic >= 0) global_avatar.set_animation_clip(ic, false);
            clip_dt = dt * 0.85f;
        }
        global_avatar.set_anim_blend(1.0f);
    } else if (clip_count == 1) {
        if (aion_current_avatar_idx == 7) {
            global_avatar.set_anim_blend(1.0f);
            if (move_speed > 0.02f || dance_active_now) {
                clip_dt = dt * (explicit_dance_active ? 0.90f : (0.72f + move_speed * 1.4f));
            } else {
                global_avatar.set_current_time(0.0f);
                clip_dt = 0.0f;
            }
        } else {
            bool walking_now = (move_speed > 0.02f);
            float target_blend = walking_now ? 1.0f : 0.0f;
            float cur_blend = global_avatar.get_anim_blend();
            cur_blend += (target_blend - cur_blend) * 0.14f;
            global_avatar.set_anim_blend(cur_blend);
            if (walking_now) {
                clip_dt = dt * (0.85f + move_speed * 2.6f);
            } else {
                float cur_t = global_avatar.get_current_time();
                float max_t = global_avatar.get_max_time();
                if (max_t > 0.05f && cur_t > 0.02f && cur_t < max_t - 0.02f) {
                    clip_dt = (cur_t > max_t * 0.5f) ? (dt * 1.2f) : (-dt * 1.2f);
                } else {
                    global_avatar.set_current_time(0.0f);
                    clip_dt = 0.0f;
                }
            }
        }
    }

    global_avatar.update(clip_dt);
    bind_avatar_bones();

    // --- Procedural Expressive Bone Choreography + Synchronous Lip-Sync ---
    if (spine_idx >= 0) {
        float breath = sinf(anim_time * 2.1f) * 0.04f;
        global_avatar.add_procedural_rotation(spine_idx, 0.0f, 0.0f, 1.0f, breath);
        if (g_dance_blend > 0.01f) {
            float sway = sinf(g_dance_phase) * 0.20f * g_dance_blend;
            float groove = sinf(g_dance_phase * 2.0f) * 0.10f * g_dance_blend;
            global_avatar.add_procedural_rotation(spine_idx, 0.0f, 1.0f, 0.0f, sway);
            global_avatar.add_procedural_rotation(spine_idx, 1.0f, 0.0f, 0.0f, groove);
        }
    }

    if (tail0_idx >= 0) {
        float tail_freq = dance_active_now ? 4.2f : (is_speaking ? 4.0f : 2.3f);
        float tail_amp  = dance_active_now ? (0.34f * fmaxf(0.55f, g_dance_blend)) : 0.22f;
        global_avatar.add_procedural_rotation(tail0_idx, 0.0f, 1.0f, 0.0f, sinf(anim_time * tail_freq) * tail_amp);
        if (tail1_idx >= 0) global_avatar.add_procedural_rotation(tail1_idx, 0.0f, 1.0f, 0.0f, sinf(anim_time * tail_freq - 0.6f) * (tail_amp * 1.2f));
        if (tail2_idx >= 0) global_avatar.add_procedural_rotation(tail2_idx, 0.0f, 1.0f, 0.0f, cosf(anim_time * tail_freq - 1.1f) * tail_amp);
    }

    if (ear_l_idx >= 0 && ear_r_idx >= 0) {
        float ear_perk = dance_active_now
            ? (sinf(g_dance_phase * 2.0f) * 0.13f * g_dance_blend)
            : (is_speaking ? (sinf(anim_time * 7.0f) * 0.12f) : (sinf(anim_time * 1.7f) * 0.05f));
        global_avatar.add_procedural_rotation(ear_l_idx, 0.0f, 0.0f, 1.0f, ear_perk);
        global_avatar.add_procedural_rotation(ear_r_idx, 0.0f, 0.0f, 1.0f, -ear_perk);
    }

    if (head_bone_idx >= 0) {
        if (g_dance_blend > 0.01f) {
            float speak_soft = is_speaking ? 0.50f : 1.0f;
            float beat_nod = sinf(g_dance_phase * 2.0f) * 0.15f * g_dance_blend * speak_soft;
            float beat_tilt = sinf(g_dance_phase) * 0.13f * g_dance_blend * speak_soft;
            global_avatar.add_procedural_rotation(head_bone_idx, 0.0f, 0.0f, 1.0f, beat_nod);
            global_avatar.add_procedural_rotation(head_bone_idx, 1.0f, 0.0f, 0.0f, beat_tilt);
        } else if (g_sit_blend > 0.05f) {
            global_avatar.add_procedural_rotation(head_bone_idx, 0.0f, 0.0f, 1.0f, -0.14f * g_sit_blend);
        }
    }

    // Synchronous WAV Lip-Sync (preserved simultaneously with dance & idle!)
    float mouth_open = compute_realtime_speech_envelope(is_speaking);
    if (jaw_idx >= 0 && mouth_open > 0.005f) {
        if (jaw_is_cat_z) {
            global_avatar.add_procedural_rotation(jaw_idx, 0.0f, 0.0f, 1.0f, mouth_open * 0.44f);
            if (tongue1_idx >= 0) global_avatar.add_procedural_rotation(tongue1_idx, 0.0f, 0.0f, 1.0f, -mouth_open * 0.18f);
            if (tongue2_idx >= 0) global_avatar.add_procedural_rotation(tongue2_idx, 0.0f, 0.0f, 1.0f, sinf(anim_time * 24.0f) * mouth_open * 0.15f);
            if (whisker_l_idx >= 0) global_avatar.add_procedural_rotation(whisker_l_idx, 0.0f, 0.0f, 1.0f, -mouth_open * 0.14f);
            if (whisker_r_idx >= 0) global_avatar.add_procedural_rotation(whisker_r_idx, 0.0f, 0.0f, 1.0f, -mouth_open * 0.14f);
            if (head_bone_idx >= 0) global_avatar.add_procedural_rotation(head_bone_idx, 0.0f, 0.0f, 1.0f, mouth_open * 0.08f * sinf(anim_time * 9.0f));
        } else if (jaw_is_rot_x_neg) {
            global_avatar.add_procedural_rotation(jaw_idx, 1.0f, 0.0f, 0.0f, mouth_open * 0.40f);
        } else {
            global_avatar.add_procedural_rotation(jaw_idx, 1.0f, 0.0f, 0.0f, mouth_open * 0.10f * sinf(anim_time * 12.0f));
        }
    }

    // Smooth Dance arm / paw choreography (paused gently on right arm only if overlay wave runs)
    if (g_dance_blend > 0.01f && g_overlay_wave_timer <= 0.05f) {
        float ph = g_dance_phase;
        float pump_r = sinf(ph) * 0.42f * g_dance_blend;
        float pump_l = -sinf(ph) * 0.42f * g_dance_blend;
        if (meta.is_mixamo_biped) {
            if (arm_r_idx >= 0)     global_avatar.add_procedural_rotation(arm_r_idx, 0.0f, 0.0f, 1.0f, (-0.55f + pump_r * 0.35f) * g_dance_blend);
            if (forearm_r_idx >= 0) global_avatar.add_procedural_rotation(forearm_r_idx, 1.0f, 0.0f, 0.0f, (-0.65f + sinf(ph * 2.0f) * 0.30f) * g_dance_blend);
            if (arm_l_idx >= 0)     global_avatar.add_procedural_rotation(arm_l_idx, 0.0f, 0.0f, 1.0f, ( 0.55f + pump_l * 0.35f) * g_dance_blend);
            if (forearm_l_idx >= 0) global_avatar.add_procedural_rotation(forearm_l_idx, 1.0f, 0.0f, 0.0f, (-0.65f - sinf(ph * 2.0f) * 0.30f) * g_dance_blend);
        } else if (meta.is_quadruped) {
            float paw_lift = explicit_dance_active ? 0.38f : 0.16f;
            if (arm_r_idx >= 0)     global_avatar.add_procedural_rotation(arm_r_idx, 0.0f, 0.0f, 1.0f, (-paw_lift - fabsf(pump_r) * 0.45f) * g_dance_blend);
            if (forearm_r_idx >= 0) global_avatar.add_procedural_rotation(forearm_r_idx, 0.0f, 0.0f, 1.0f, (0.28f + pump_r * 0.25f) * g_dance_blend);
            if (arm_l_idx >= 0)     global_avatar.add_procedural_rotation(arm_l_idx, 0.0f, 0.0f, 1.0f, (-paw_lift - fabsf(pump_l) * 0.45f) * g_dance_blend);
            if (forearm_l_idx >= 0) global_avatar.add_procedural_rotation(forearm_l_idx, 0.0f, 0.0f, 1.0f, (0.28f + pump_l * 0.25f) * g_dance_blend);
        }
    }

    // Friendly wave paw/arm (standalone OR overlay while continuing dance!)
    if (g_wave_blend > 0.01f && (g_action_state == AV_WAVE || g_overlay_wave_timer > 0.0f)) {
        float w = sinf(anim_time * 9.0f) * 0.42f * g_wave_blend;
        if (meta.is_mixamo_biped) {
            if (arm_r_idx >= 0)     global_avatar.add_procedural_rotation(arm_r_idx, 0.0f, 0.0f, 1.0f, -1.65f * g_wave_blend);
            if (forearm_r_idx >= 0) global_avatar.add_procedural_rotation(forearm_r_idx, 1.0f, 0.0f, 0.0f, -0.85f * g_wave_blend + w);
        } else {
            if (arm_r_idx >= 0)     global_avatar.add_procedural_rotation(arm_r_idx, 0.0f, 0.0f, 1.0f, -(0.95f + w * 0.35f) * g_wave_blend);
            if (forearm_r_idx >= 0) global_avatar.add_procedural_rotation(forearm_r_idx, 0.0f, 0.0f, 1.0f, (0.45f + w) * g_wave_blend);
            if (hand_r_idx >= 0)    global_avatar.add_procedural_rotation(hand_r_idx, 0.0f, 0.0f, 1.0f, w * 0.7f);
        }
    }

    // Conversational pointing / gesture while explaining
    if (g_point_blend > 0.01f && g_action_state != AV_DANCE && g_action_state != AV_WAVE) {
        float tap = sinf(anim_time * 5.0f) * 0.14f;
        float lift = -(0.52f + tap) * g_point_blend;
        float ext  = (0.30f + tap * 0.4f) * g_point_blend;
        int pa = (g_point_side == 0) ? arm_r_idx : arm_l_idx;
        int pf = (g_point_side == 0) ? forearm_r_idx : forearm_l_idx;
        if (pa >= 0) {
            if (meta.is_mixamo_biped) {
                global_avatar.add_procedural_rotation(pa, 0.0f, 0.0f, 1.0f, (g_point_side == 0 ? -0.45f : 0.45f) * g_point_blend);
                if (pf >= 0) global_avatar.add_procedural_rotation(pf, 1.0f, 0.0f, 0.0f, -(0.45f + tap) * g_point_blend);
            } else {
                global_avatar.add_procedural_rotation(pa, 0.0f, 0.0f, 1.0f, lift);
                if (pf >= 0) global_avatar.add_procedural_rotation(pf, 0.0f, 0.0f, 1.0f, ext);
            }
        }
    }

    // Sitting hind-leg tuck when rigged
    if (g_sit_blend > 0.01f && meta.is_quadruped) {
        if (leg_l_idx >= 0) global_avatar.add_procedural_rotation(leg_l_idx, 0.0f, 0.0f, 1.0f, 0.55f * g_sit_blend);
        if (leg_r_idx >= 0) global_avatar.add_procedural_rotation(leg_r_idx, 0.0f, 0.0f, 1.0f, 0.55f * g_sit_blend);
    }

    global_avatar.recompute_global_matrices();
    global_avatar.skin();

    // Update exposed screen pixel coordinates + status string for LLM self-awareness
    aion_avatar_screen_x = (int)((g_pos_sx + 1.0f) * 0.5f * global_screen_w);
    aion_avatar_screen_y = (int)((1.0f - g_pos_sy) * 0.5f * global_screen_h);
    {
        std::lock_guard<std::mutex> lock(g_status_mutex);
        snprintf(g_status_buf, sizeof(g_status_buf),
                 "Model %d/%d (%s) | Action: %s | Pos=(%.2f,%.2f) | Zoom=%.2f",
                 aion_current_avatar_idx + 1,
                 (aion_total_models > 0) ? aion_total_models : kNumKnownModels,
                 meta.name,
                 action_state_name(g_action_state),
                 g_pos_sx, g_pos_sy, aion_avatar_zoom);
    }

    // --- OpenGL Render (true Y-up glTF world space, feet-anchored projection) ---
    float aspect = (float)global_screen_w / (float)global_screen_h;
    float fov = 45.0f * (3.14159f / 180.0f);
    const float cam_dist = 3.2f;
    float half_h = tanf(fov * 0.5f) * cam_dist;
    float half_w = half_h * aspect;

    float world_x = g_pos_sx * half_w + aion_avatar_pan_x + extra_shift_x;
    float world_y = g_pos_sy * half_h + aion_avatar_pan_y + extra_lift_y;

    float final_rot_x = aion_avatar_rot_x + g_body_pitch_offset;
    float final_rot_y = aion_avatar_rot_y + g_body_yaw_offset;
    float final_rot_z = aion_avatar_rot_z + g_body_roll_offset;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glViewport(0, 0, global_screen_w, global_screen_h);
    glClear(GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float zNear = 0.1f, zFar = 1000.0f;
    float fH = tanf(fov * 0.5f) * zNear;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 1. Place feet anchor cleanly in 3D space at fixed cam_dist
    glTranslatef(world_x, world_y, -cam_dist);
    // 2. Apply upright orientation & choreographed squash/stretch around feet pivot
    glRotatef(final_rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(final_rot_y, 0.0f, 1.0f, 0.0f);
    glRotatef(final_rot_z, 0.0f, 0.0f, 1.0f);
    glScalef(aion_avatar_zoom * scale_mod_xz, aion_avatar_zoom * scale_mod_y, aion_avatar_zoom * scale_mod_xz);
    // 3. Center & auto-scale model directly in upright glTF +Y up space (zero -90 X hack!)
    glScalef(global_avatar.auto_scale, global_avatar.auto_scale, global_avatar.auto_scale);
    glTranslatef(global_avatar.center_offset.x, global_avatar.center_offset.y, global_avatar.center_offset.z);

    global_avatar.draw();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();

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
