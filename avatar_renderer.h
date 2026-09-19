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

struct A3DFrame {
    uint32_t num_triangles;
    float* vertices; // 15 floats per triangle (x,y,z, u,v) * 3
};

struct A3DModel {
    uint32_t num_frames;
    A3DFrame* frames;
    bool loaded = false;
};

A3DModel global_avatar;

void load_a3d(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Failed to load %s\n", path);
        return;
    }
    
    char magic[4];
    fread(magic, 1, 4, f);
    if (magic[0] != 'A' || magic[1] != '3' || magic[2] != 'D' || magic[3] != '!') {
        printf("Invalid A3D file %s\n", path);
        fclose(f);
        return;
    }
    
    fread(&global_avatar.num_frames, 4, 1, f);
    uint32_t res[2];
    fread(res, 4, 2, f);
    
    global_avatar.frames = new A3DFrame[global_avatar.num_frames];
    for (uint32_t i = 0; i < global_avatar.num_frames; i++) {
        fread(&global_avatar.frames[i].num_triangles, 4, 1, f);
        uint32_t num_floats = global_avatar.frames[i].num_triangles * 15;
        global_avatar.frames[i].vertices = new float[num_floats];
        fread(global_avatar.frames[i].vertices, 4, num_floats, f);
    }
    
    global_avatar.loaded = true;
    fclose(f);
    printf("Successfully loaded A3D model with %d frames\n", global_avatar.num_frames);
}

void draw_a3d() {
    if (!global_avatar.loaded || !aion_window_open || !aion_use_3d_avatar) return;
    
    static float current_frame = 0;
    
    // Animation Speed relative to Bass / Frequency
    float speed = 0.5f + (linux_current_freq * 0.02f); // Dance to music!
    current_frame += speed;
    if (current_frame >= global_avatar.num_frames) current_frame = 0;
    
    uint32_t f_idx = (uint32_t)current_frame;
    if (f_idx >= global_avatar.num_frames) f_idx = 0;
    
    A3DFrame* f = &global_avatar.frames[f_idx];
    if (f->num_triangles == 0) return;
    
    // 3D Projection Setup inside the AION NEXUS window
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    
    // Calculate OpenGL window coordinates (Y is flipped in OpenGL)
    extern int global_screen_w;
    extern int global_screen_h;
    int scissor_y = global_screen_h - (aion_window_y + aion_window_h);
    glScissor(aion_window_x, scissor_y, aion_window_w, aion_window_h);
    glViewport(aion_window_x, scissor_y, aion_window_w, aion_window_h);
    
    glClear(GL_DEPTH_BUFFER_BIT); // Clear depth only for the window area
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Simple perspective
    float aspect = (float)aion_window_w / (float)aion_window_h;
    float fov = 45.0f * (3.14159f / 180.0f);
    float zNear = 0.1f;
    float zFar = 1000.0f;
    float fH = tanf(fov / 2.0f) * zNear;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Camera Transform (Looking at the avatar)
    glTranslatef(0.0f, -1.0f, -3.0f);
    
    // Slight rotation
    static float rot = 0;
    rot += 0.5f;
    glRotatef(rot, 0, 1, 0);
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 5 * sizeof(float), f->vertices);
    
    // Draw wireframe for cool matrix effect (since textures aren't fully loaded yet)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(0.0f, 1.0f, 0.5f); // Neon Green
    
    glDrawArrays(GL_TRIANGLES, 0, f->num_triangles * 3);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    
    // Reset Viewport to Full Screen
    glViewport(0, 0, global_screen_w, global_screen_h);
}
