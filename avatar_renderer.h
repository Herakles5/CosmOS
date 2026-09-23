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
    }

    if (!global_avatar.is_loaded() || !aion_window_open || !aion_use_3d_avatar) return;
    
    // Animation Speed relative to Bass / Frequency
    float speed = 0.5f + (linux_current_freq * 0.02f);
    global_avatar.update(0.016f * speed);
    global_avatar.skin();
    
    extern int global_screen_w;
    extern int global_screen_h;
    extern int mouse_x;
    extern int mouse_y;
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
    
    // Target rotation angles - more subtle (max ±15/10 degrees follow) so it doesn't "stick"
    float target_rot_y = dx * 15.0f;
    float target_rot_x = dy * 10.0f;
    
    // Check if user is manually dragging (override follow)
    if (mouse_down && mouse_x > (int)aion_window_x && mouse_x < (int)(aion_window_x + aion_window_w) &&
        mouse_y > (int)(aion_window_y + 30) && mouse_y < (int)(aion_window_y + aion_window_h - 220)) {
        aion_mouse_dragging = true;
    } else if (!mouse_down && aion_mouse_dragging) {
        aion_mouse_dragging = false;
        aion_drag_release_time = 120; // Wait ~2 seconds before re-engaging follow
    }
    
    // Countdown after drag release
    if (aion_drag_release_time > 0) aion_drag_release_time--;
    
    // Smooth follow (only when not manually dragging and cooldown expired)
    float lerp_speed = 0.015f; // Slower, more elegant follow
    if (!aion_mouse_dragging && aion_drag_release_time == 0) {
        aion_follow_rot_y += (target_rot_y - aion_follow_rot_y) * lerp_speed;
        aion_follow_rot_x += (target_rot_x - aion_follow_rot_x) * lerp_speed;
    }
    
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
    
    // The console takes up the bottom 220 pixels, title bar top 30 pixels
    int draw_h = aion_window_h - 220 - 30;
    if (draw_h < 1) draw_h = 1;
    int scissor_y = global_screen_h - (aion_window_y + aion_window_h - 220);
    
    // Allow rendering outside window by expanding the viewport and frustum by 3x
    int vp_w = aion_window_w * 3;
    int vp_h = draw_h * 3;
    int vp_x = aion_window_x - aion_window_w;
    int vp_y = scissor_y - draw_h;
    
    glViewport(vp_x, vp_y, vp_w, vp_h);
    
    glClear(GL_DEPTH_BUFFER_BIT); // Depth buffer clear for the whole expanded area
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)aion_window_w / (float)draw_h;
    float fov = 45.0f * (3.14159f / 180.0f);
    float zNear = 0.1f;
    float zFar = 1000.0f;
    float fH = tanf(fov / 2.0f) * zNear;
    float fW = fH * aspect;
    
    // Scale frustum by 3 to perfectly counteract the 3x viewport size
    glFrustum(-fW * 3.0f, fW * 3.0f, -fH * 3.0f, fH * 3.0f, zNear, zFar);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Camera Transform (center of the screen)
    glTranslatef(0.0f + aion_avatar_pan_x, -0.75f + aion_avatar_pan_y, -3.0f * (1.0f / aion_avatar_zoom));
    
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

