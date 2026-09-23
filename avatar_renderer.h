#pragma once
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

extern "C" uint32_t aion_window_x;
extern "C" uint32_t aion_window_y;
extern "C" uint32_t aion_window_w;
extern "C" uint32_t aion_window_h;
extern "C" uint8_t  aion_window_open;
extern "C" uint8_t  aion_use_3d_avatar;
extern uint32_t linux_current_freq;
// Lock removed - 3D interaction is always active
float aion_avatar_zoom = 1.0f;
float aion_avatar_rot_x = 0.0f;
float aion_avatar_rot_y = 0.0f;
float aion_avatar_rot_z = 0.0f;
float aion_avatar_pan_x = 0.0f;
float aion_avatar_pan_y = 0.0f;
float aion_manual_pan_x = 0.0f;
float aion_manual_pan_y = 0.0f;
int aion_avatar_screen_x = 0;
int aion_avatar_screen_y = 0;
int jaw_idx = -2;
int spine_idx = -2;
int arm_r_idx = -2;
int arm_l_idx = -2;
float anim_time = 0.0f;

#include "avatar_anim.h"

SkeletalAvatar global_avatar;

extern "C" uint8_t  aion_switch_avatar = 0;
extern "C" int      aion_current_avatar_idx = 0;
extern "C" int      aion_total_models = 0;

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
        aion_avatar_zoom = 1.0f;
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

// Mouse-follow state
static float aion_follow_rot_x = 0.0f;
static float aion_follow_rot_y = 0.0f;
static bool  aion_mouse_dragging = false;
static uint64_t aion_drag_release_time = 0;

void draw_a3d() {
    if (aion_switch_avatar) {
        aion_switch_avatar = 0;
        char path[256];
        snprintf(path, 256, "/opt/meinos/model_%d.glb", aion_current_avatar_idx);
        load_a3d(path);
        aion_load_config(aion_current_avatar_idx);
        
        // Reset pan and rotation on switch so it spawns in the center of its window
        aion_avatar_pan_x = 0.0f;
        aion_avatar_pan_y = 0.0f;
        aion_avatar_rot_x = 0.0f;
        aion_avatar_rot_y = 0.0f;
        aion_manual_pan_x = 0.0f;
        aion_manual_pan_y = 0.0f;
        
        // Reset bone indices so it searches for the correct bones in the new model
        jaw_idx = -2;
        spine_idx = -2;
        arm_l_idx = -2;
        arm_r_idx = -2;
    }

    if (!global_avatar.is_loaded() || !aion_window_open || !aion_use_3d_avatar) return;
    
    // Animation Speed relative to Bass / Frequency
    float speed = 0.5f + (linux_current_freq * 0.02f);
    global_avatar.update(0.016f * speed);
    
    extern int global_screen_w;
    extern int global_screen_h;
    extern uint32_t mouse_x;
    extern uint32_t mouse_y;
    extern bool mouse_down;
    
    // --- Smooth Mouse Follow ---
    // Calculate center of the 3D viewport on screen
    float model_center_x = aion_window_x + aion_window_w * 0.5f;
    float model_center_y = aion_window_y + 30 + (aion_window_h - 220 - 30) * 0.5f;
    
    // Vector from model center to mouse cursor (normalized to -1..1 range)
    float dx = ((float)mouse_x - model_center_x) / (float)global_screen_w * 2.0f;
    float dy = ((float)mouse_y - model_center_y) / (float)global_screen_h * 2.0f;
    
    // Clamp to reasonable range
    if (dx < -1.0f) dx = -1.0f; if (dx > 1.0f) dx = 1.0f;
    if (dy < -1.0f) dy = -1.0f; if (dy > 1.0f) dy = 1.0f;
    
    // Frustum dimensions at Z = -3.0f (must match the glFrustum setup below)
    float aspect = (float)global_screen_w / (float)global_screen_h;
    float fov = 45.0f * (3.14159f / 180.0f);
    float half_h_at_z3 = tanf(fov / 2.0f) * 3.0f;
    float half_w_at_z3 = half_h_at_z3 * aspect;
    
    // Calculate AION window center in normalized screen coordinates
    float nx = (model_center_x / (float)global_screen_w) * 2.0f - 1.0f;
    float ny = 1.0f - (model_center_y / (float)global_screen_h) * 2.0f;
    float baseline_x = nx * half_w_at_z3;
    float baseline_y = ny * half_h_at_z3;

    // Calculate avatar screen coordinates for her 'invisible window frame'
    float current_world_x = baseline_x + aion_avatar_pan_x + aion_manual_pan_x;
    float current_world_y = baseline_y - 0.75f + aion_avatar_pan_y + aion_manual_pan_y;
    float current_nx = current_world_x / half_w_at_z3;
    float current_ny = current_world_y / half_h_at_z3;
    aion_avatar_screen_x = (int)((current_nx + 1.0f) * 0.5f * global_screen_w);
    aion_avatar_screen_y = (int)((1.0f - current_ny) * 0.5f * global_screen_h);
    
    // Target rotation angles
    float target_rot_y = dx * 45.0f;
    float target_rot_x = dy * 20.0f;
    
    // Check if user is manually dragging (override follow)
    bool over_aion_window = mouse_x > (uint32_t)aion_window_x && mouse_x < (uint32_t)(aion_window_x + aion_window_w) &&
                            mouse_y > (uint32_t)(aion_window_y + 30) && mouse_y < (uint32_t)(aion_window_y + aion_window_h - 220);
    bool over_avatar_frame = mouse_x > (uint32_t)(aion_avatar_screen_x - 150) && mouse_x < (uint32_t)(aion_avatar_screen_x + 150) &&
                             mouse_y > (uint32_t)(aion_avatar_screen_y - 400) && mouse_y < (uint32_t)(aion_avatar_screen_y + 100);
                             
    extern bool mouse_right_down;
    if ((mouse_down || mouse_right_down) && (over_aion_window || over_avatar_frame)) {
        aion_mouse_dragging = true;
    } else if (!mouse_down && !mouse_right_down && aion_mouse_dragging) {
        aion_mouse_dragging = false;
        aion_drag_release_time = 120; // Wait ~2 seconds before re-engaging follow
    }
    
    // Countdown after drag release
    if (aion_drag_release_time > 0) aion_drag_release_time--;
    
    // Smooth follow (rotation)
    float lerp_speed = 0.05f;
    if (!aion_mouse_dragging && aion_drag_release_time == 0) {
        aion_follow_rot_y += (target_rot_y - aion_follow_rot_y) * lerp_speed;
        aion_follow_rot_x += (target_rot_x - aion_follow_rot_x) * lerp_speed;
        
        // --- Free Screen Movement (Translation) ---
        float mouse_nx = ((float)mouse_x / (float)global_screen_w) * 2.0f - 1.0f;
        float mouse_ny = 1.0f - ((float)mouse_y / (float)global_screen_h) * 2.0f;
        
        float target_world_x = mouse_nx * half_w_at_z3;
        float target_world_y = mouse_ny * half_h_at_z3;
        
        float target_pan_x = target_world_x - baseline_x;
        float target_pan_y = target_world_y - baseline_y;
        
        float move_speed = 0.05f; // Walk speed
        aion_avatar_pan_x += (target_pan_x - aion_avatar_pan_x) * move_speed;
        aion_avatar_pan_y += (target_pan_y - aion_avatar_pan_y) * move_speed;
    }
    
    // Check if speaking
    FILE* sf = fopen("/tmp/aion_speaking", "r");
    bool is_speaking = false;
    if (sf) {
        is_speaking = true;
        fclose(sf);
    }
    
    // Procedural Animations (BEFORE update)
    anim_time += 0.016f;
    
    if (jaw_idx == -2) {
        jaw_idx = global_avatar.find_bone("Jaw");
        if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("jaw");
        if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("mixamorig:Jaw");
        if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("Head_");
        if (jaw_idx == -1) jaw_idx = global_avatar.find_bone("Head");
    }
    if (spine_idx == -2) {
        spine_idx = global_avatar.find_bone("Spine");
        if (spine_idx == -1) spine_idx = global_avatar.find_bone("spine");
        if (spine_idx == -1) spine_idx = global_avatar.find_bone("mixamorig:Spine");
        if (spine_idx == -1) spine_idx = global_avatar.find_bone("Chest");
    }
    if (arm_r_idx == -2) {
        arm_r_idx = global_avatar.find_bone("RightArm");
        if (arm_r_idx == -1) arm_r_idx = global_avatar.find_bone("mixamorig:RightArm");
    }
    if (arm_l_idx == -2) {
        arm_l_idx = global_avatar.find_bone("LeftArm");
        if (arm_l_idx == -1) arm_l_idx = global_avatar.find_bone("mixamorig:LeftArm");
    }
    
    if (spine_idx >= 0) {
        float breath = sinf(anim_time * 2.0f) * 0.05f;
        global_avatar.add_procedural_rotation(spine_idx, 1.0f, 0.0f, 0.0f, breath * 2.0f);
    }
    
    if (jaw_idx >= 0 && is_speaking) {
        float talk = (sinf(anim_time * 15.0f) + sinf(anim_time * 22.0f) * 0.5f) * 0.15f + 0.15f;
        global_avatar.add_procedural_rotation(jaw_idx, 1.0f, 0.0f, 0.0f, talk * 2.0f);
    }
    
    if (arm_r_idx >= 0 && is_speaking) {
        float wave = sinf(anim_time * 3.0f) * 0.3f;
        global_avatar.add_procedural_rotation(arm_r_idx, 0.0f, 0.0f, 1.0f, wave * 2.0f);
    }
    
    if (arm_l_idx >= 0 && is_speaking) {
        float wave = sinf(anim_time * 4.0f + 1.0f) * -0.3f;
        global_avatar.add_procedural_rotation(arm_l_idx, 0.0f, 0.0f, 1.0f, wave * 2.0f);
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
    glTranslatef(baseline_x + aion_avatar_pan_x + aion_manual_pan_x, baseline_y - 0.75f + aion_avatar_pan_y + aion_manual_pan_y, -3.0f * (1.0f / aion_avatar_zoom));
    
    // Apply combined rotations (manual + mouse follow)
    glRotatef(final_rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(final_rot_y, 0.0f, 1.0f, 0.0f);
    glRotatef(aion_avatar_rot_z, 0.0f, 0.0f, 1.0f);
    
    // Apply model's auto-centering and scaling (applied AFTER Z-to-Y fix)
    glScalef(global_avatar.auto_scale, global_avatar.auto_scale, global_avatar.auto_scale);
    glTranslatef(global_avatar.center_offset.x, global_avatar.center_offset.y, global_avatar.center_offset.z);
    
    // Fix Z-up to Y-up (applied FIRST to raw vertices)
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    
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

