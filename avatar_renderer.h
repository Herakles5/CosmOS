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

void load_a3d(const char* path) {
    global_avatar.load_glb(path);
    if (global_avatar.is_loaded()) {
        printf("Successfully loaded GLB model %s\n", path);
    } else {
        printf("Failed to load GLB model %s\n", path);
    }
}

void draw_a3d() {
    if (aion_switch_avatar) {
        aion_switch_avatar = 0;
        char path[256];
        snprintf(path, 256, "/opt/meinos/model_%d.glb", aion_current_avatar_idx);
        load_a3d(path);
    }

    if (!global_avatar.is_loaded() || !aion_window_open || !aion_use_3d_avatar) return;
    
    // Animation Speed relative to Bass / Frequency
    float speed = 0.5f + (linux_current_freq * 0.02f);
    global_avatar.update(0.016f * speed);
    global_avatar.skin();
    
    extern int global_screen_w;
    extern int global_screen_h;
    
    // SAVE entire OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    
    // 3D Projection Setup inside the AION NEXUS window
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    // The console takes up the bottom 220 pixels, title bar top 30 pixels
    int draw_h = aion_window_h - 220 - 30;
    if (draw_h < 1) draw_h = 1;
    int scissor_y = global_screen_h - (aion_window_y + aion_window_h - 220);
    
    glScissor(aion_window_x, scissor_y, aion_window_w, draw_h);
    glViewport(aion_window_x, scissor_y, aion_window_w, draw_h);
    
    glClear(GL_DEPTH_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)aion_window_w / (float)draw_h;
    float fov = 45.0f * (3.14159f / 180.0f);
    float zNear = 0.1f;
    float zFar = 1000.0f;
    float fH = tanf(fov / 2.0f) * zNear;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Camera Transform (center of the screen)
    // The auto_scale normalizes model height to 1.5 units, with feet at Y=0.
    // So we translate Y by -0.75 to center it vertically.
    glTranslatef(0.0f + aion_avatar_pan_x, -0.75f + aion_avatar_pan_y, -3.0f * (1.0f / aion_avatar_zoom));
    
    // Apply User Rotations
    glRotatef(aion_avatar_rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(aion_avatar_rot_y, 0.0f, 1.0f, 0.0f);
    glRotatef(aion_avatar_rot_z, 0.0f, 0.0f, 1.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    
    // Apply model's auto-centering and scaling
    glScalef(global_avatar.auto_scale, global_avatar.auto_scale, global_avatar.auto_scale);
    glTranslatef(global_avatar.center_offset.x, global_avatar.center_offset.y, global_avatar.center_offset.z);
    
    // Draw the GLB model
    global_avatar.draw();
    
    // RESTORE OpenGL state (pop saved state)
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    
    // EXPLICIT RESET of all critical state for safety
    // (some GL drivers don't restore everything with glPopAttrib)
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
