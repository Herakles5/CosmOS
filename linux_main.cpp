#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <thread>
extern int mouse_wheel;
// Globals used by app.cpp and kernel_linux.cpp
extern uint32_t input_cooldown;
extern bool mouse_just_pressed;
extern uint32_t mouse_x;
extern uint32_t mouse_y;
extern bool mouse_right_down;
extern uint8_t key_scancode;
extern uint32_t frame;
// --- BARE METAL 3D FIX: VARIABLEN BEKANNT MACHEN ---
extern volatile int os_app_x;
extern volatile int os_app_y;
extern volatile int os_app_w;
extern volatile int os_app_h;
extern "C" void run_smash_cats_engine(int wx, int wy, int ww, int wh, bool is_blocked);

extern "C" {
    // Provided by kernel_main_linux.cpp
    extern volatile bool app_window_active;
    extern uint32_t app_mouse_x, app_mouse_y;
    extern int meow_timer;
    extern uint32_t* fb;
    
    extern uint32_t linux_mx, linux_my;
    extern bool linux_mdown;
    extern uint8_t linux_scancode;
    
    extern volatile uint64_t system_ticks;
    extern volatile uint64_t os2_system_ticks;
    
    uint8_t linux_wav_buffer[10 * 1024 * 1024];
    uint32_t linux_wav_len = 0;
    uint32_t linux_wav_pos = 0;
    
    void play_hda_wav(uint64_t pcm_addr, uint32_t size_bytes, uint16_t sample_rate, uint16_t channels, uint16_t bits) {
        if (size_bytes > sizeof(linux_wav_buffer)) size_bytes = sizeof(linux_wav_buffer);
        // Copy if not already using the buffer directly
        if ((uint64_t)linux_wav_buffer != pcm_addr) {
            memcpy(linux_wav_buffer, (void*)pcm_addr, size_bytes);
        }
        linux_wav_len = size_bytes;
        linux_wav_pos = 0;
    }
    
    // Global buffers for kernel_main_linux.cpp to avoid segfaults
    uint8_t global_buf_mbr[4096];
    uint8_t global_buf_dir[4096];
    uint8_t global_tmp_dir[4096];
}

// Exported functions
extern "C" void cosmos_main(void* boot_info);
extern void run_smash_cats_engine(int wx, int wy, int ww, int wh, bool is_blocked);

// SDL keycode to PS/2 scancode conversion
static uint8_t sdl_to_ps2(SDL_Keycode key) {
    switch (key) {
        case SDLK_ESCAPE: return 0x01;
        case SDLK_1: return 0x02; case SDLK_2: return 0x03; case SDLK_3: return 0x04;
        case SDLK_4: return 0x05; case SDLK_5: return 0x06; case SDLK_6: return 0x07;
        case SDLK_7: return 0x08; case SDLK_8: return 0x09; case SDLK_9: return 0x0A;
        case SDLK_0: return 0x0B;
        case SDLK_MINUS: return 0x0C; case SDLK_EQUALS: return 0x0D;
        case SDLK_BACKSPACE: return 0x0E; case SDLK_TAB: return 0x0F;
        case SDLK_q: return 0x10; case SDLK_w: return 0x11; case SDLK_e: return 0x12;
        case SDLK_r: return 0x13; case SDLK_t: return 0x14; case SDLK_z: return 0x15;
        case SDLK_u: return 0x16; case SDLK_i: return 0x17; case SDLK_o: return 0x18;
        case SDLK_p: return 0x19;
        case SDLK_RETURN: return 0x1C;
        case SDLK_a: return 0x1E; case SDLK_s: return 0x1F; case SDLK_d: return 0x20;
        case SDLK_f: return 0x21; case SDLK_g: return 0x22; case SDLK_h: return 0x23;
        case SDLK_j: return 0x24; case SDLK_k: return 0x25; case SDLK_l: return 0x26;
        case SDLK_y: return 0x2C; case SDLK_x: return 0x2D; case SDLK_c: return 0x2E;
        case SDLK_v: return 0x2F; case SDLK_b: return 0x30; case SDLK_n: return 0x31;
        case SDLK_m: return 0x32;
        case SDLK_COMMA: return 0x33; case SDLK_PERIOD: return 0x34;
        case SDLK_SLASH: return 0x35;
        case SDLK_SPACE: return 0x39;
        case SDLK_F1: return 0x3B; case SDLK_F2: return 0x3C; case SDLK_F3: return 0x3D;
        case SDLK_F4: return 0x3E; case SDLK_F5: return 0x3F; case SDLK_F6: return 0x40;
        case SDLK_F7: return 0x41; case SDLK_F8: return 0x42; case SDLK_F9: return 0x43;
        case SDLK_F10: return 0x44; case SDLK_F11: return 0x85; case SDLK_F12: return 0x86;
        case SDLK_UP: return 0x48; case SDLK_DOWN: return 0x50;
        case SDLK_LEFT: return 0x4B; case SDLK_RIGHT: return 0x4D;
        default: return 0;
    }
}

#include <math.h>
#include <stdlib.h>

// Globals for simple tone generation
uint32_t linux_current_freq = 0;
uint32_t linux_sound_duration = 0; // in samples (or just ms)
uint32_t linux_sound_max_duration = 1;

// WAV Sound Buffers
Uint32 cat_wav_len[4] = {0};
Uint8 *cat_wav_buf[4] = {NULL};
Uint32 active_cat_wav_idx = (Uint32)-1;
Uint32 active_cat_wav_pos = 0;

void load_and_convert_wav(const char* file, int index, SDL_AudioSpec* target_spec) {
    SDL_AudioSpec wav_spec;
    Uint32 wav_length;
    Uint8 *wav_buffer;
    if (SDL_LoadWAV(file, &wav_spec, &wav_buffer, &wav_length) == NULL) {
        printf("Failed to load %s: %s\n", file, SDL_GetError());
        return;
    }
    
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, wav_spec.format, wav_spec.channels, wav_spec.freq, 
                          target_spec->format, target_spec->channels, target_spec->freq) < 0) {
        printf("Failed to build CVT for %s\n", file);
        SDL_FreeWAV(wav_buffer);
        return;
    }
    
    cvt.len = wav_length;
    cvt.buf = (Uint8 *)malloc(cvt.len * cvt.len_mult);
    memcpy(cvt.buf, wav_buffer, wav_length);
    SDL_ConvertAudio(&cvt);
    SDL_FreeWAV(wav_buffer);
    
    cat_wav_buf[index] = cvt.buf;
    cat_wav_len[index] = cvt.len_cvt;
    printf("Loaded %s (Len: %d)\n", file, cvt.len_cvt);
}

extern "C" void play_linux_freq(uint32_t freq) {
    linux_current_freq = freq;
    linux_sound_duration = 0xFFFFFFFF; // Play indefinitely until play_freq(0) is called
    linux_sound_max_duration = 1;
}

extern "C" void play_linux_sound(uint32_t freq, uint32_t duration_ms) {
    linux_current_freq = freq;
    linux_sound_duration = (44100 * duration_ms) / 1000;
    linux_sound_max_duration = linux_sound_duration;
}

// Audio Callback
void audio_callback(void* userdata, Uint8* stream, int len) {
    static float phase_left = 0.0f;
    static float phase_right = 0.0f;
    int16_t* out = (int16_t*)stream;
    int num_frames = len / 4; // 16-bit stereo = 4 bytes per frame
    
    if (linux_wav_len > 0 && linux_wav_pos < linux_wav_len) {
        int bytes_to_copy = len;
        if (linux_wav_pos + bytes_to_copy > linux_wav_len) {
            bytes_to_copy = linux_wav_len - linux_wav_pos;
        }
        memcpy(stream, linux_wav_buffer + linux_wav_pos, bytes_to_copy);
        linux_wav_pos += bytes_to_copy;
        
        if (bytes_to_copy < len) {
            memset(stream + bytes_to_copy, 0, len - bytes_to_copy);
            linux_wav_len = 0;
        }
        goto apply_surreal;
    }

    // Initialize Stream to 0 to prepare for mixing
    memset(stream, 0, len);
    
    // --- 1. SYSTEM BEEP / SURREAL BEEP ---
    if (linux_current_freq > 0 && linux_sound_duration > 0) {
        float phase_inc_left = (float)linux_current_freq / 44100.0f;
        float phase_inc_right = ((float)linux_current_freq * 1.015f) / 44100.0f; 
        extern bool cfg_sound_surreal;
        
        for (int i=0; i<num_frames; i++) {
            phase_left += phase_inc_left;
            if (phase_left > 1.0f) phase_left -= 1.0f;
            
            if (!cfg_sound_surreal) {
                // CLASSIC CHIPTUNE
                float sq = (phase_left < 0.5f) ? 1.0f : -1.0f;
                int16_t val = (int16_t)(sq * 4000.0f);
                out[i*2] = val; 
                out[i*2+1] = val;
            } else {
                // SURREAL SOUND
                phase_right += phase_inc_right;
                if (phase_right > 1.0f) phase_right -= 1.0f;
                
                float t = 1.0f;
                if (linux_sound_max_duration > 0 && linux_sound_max_duration != 0xFFFFFFFF) {
                    t = (float)linux_sound_duration / (float)linux_sound_max_duration;
                }
                
                float hit_env = t * t * t * t; 
                float mist_env = t * t * 0.3f;
                
                float sq_left = (phase_left < 0.5f) ? 1.0f : -1.0f;
                float saw_right = (phase_right * 2.0f) - 1.0f;
                float sine_mist = sin(phase_left * 6.28318f);
                float noise = ((float)(rand() % 1000) / 500.0f) - 1.0f;
                
                float left_mix = (sq_left * hit_env * 0.5f) + (sine_mist * mist_env) + (noise * mist_env * 0.2f);
                float right_mix = (saw_right * hit_env * 0.5f) + (sine_mist * mist_env) + (noise * mist_env * 0.2f);
                
                out[i*2]   = (int16_t)(left_mix * 8000.0f);
                out[i*2+1] = (int16_t)(right_mix * 8000.0f);
            }
            
            if (linux_sound_duration != 0xFFFFFFFF) {
                linux_sound_duration--;
                if (linux_sound_duration == 0) {
                    linux_current_freq = 0;
                    break;
                }
            }
        }
    }
    
    // --- 2. CAT WAV SOUNDS ---
    if (meow_timer > 0) {
        extern int meow_type;
        // Map meow_type to wav index
        if (meow_type == 0) active_cat_wav_idx = 0; // cat_meow.wav
        else if (meow_type == 1) active_cat_wav_idx = 1; // nervous_cat.wav
        else if (meow_type == 2 || meow_type == 3) active_cat_wav_idx = 2; // cat_unhappy.wav
        else active_cat_wav_idx = 3; // furious_cat.wav
        
        active_cat_wav_pos = 0;
        meow_timer = 0; // Clear the trigger
    }
    
    if (active_cat_wav_idx != (Uint32)-1 && cat_wav_buf[active_cat_wav_idx] != NULL) {
        Uint32 remaining = cat_wav_len[active_cat_wav_idx] - active_cat_wav_pos;
        if (remaining > 0) {
            Uint32 bytes_to_mix = (len > remaining) ? remaining : len;
            int16_t* src = (int16_t*)(cat_wav_buf[active_cat_wav_idx] + active_cat_wav_pos);
            
            for (Uint32 i = 0; i < bytes_to_mix / 2; i++) {
                int32_t mixed = out[i] + src[i]; // Additive mix
                if (mixed > 32767) mixed = 32767;
                if (mixed < -32768) mixed = -32768;
                out[i] = (int16_t)mixed;
            }
            active_cat_wav_pos += bytes_to_mix;
        } else {
            active_cat_wav_idx = (Uint32)-1;
        }
    }

apply_surreal:
    // --- 3. GLOBAL SURREAL EFFECT FILTER ---
    // User requested the surreal effect to apply to ALL audio outputs (music, sounds, etc)
    extern bool cfg_sound_surreal;
    if (cfg_sound_surreal) {
        static float surreal_phase = 0.0f;
        for (int i = 0; i < len / 4; i++) {
            int16_t left = out[i*2];
            int16_t right = out[i*2+1];
            
            // Only apply surreal effect when there is actual audio signal
            // This prevents the noise/hiss from playing during silence
            if (left == 0 && right == 0) continue;
            
            surreal_phase += 0.005f;
            if (surreal_phase > 6.28318f) surreal_phase -= 6.28318f;
            
            float mist = sin(surreal_phase); 
            float noise = ((float)(rand() % 1000) / 500.0f) - 1.0f;
            
            // Scale noise by the signal amplitude so it blends naturally
            float signal_level = (float)(abs(left) + abs(right)) / 65536.0f;
            
            // Apply alien/surreal ring modulation and signal-proportional noise
            int32_t mixed_l = (int32_t)(left * (0.7f + 0.3f * mist)) + (int32_t)(noise * 500.0f * signal_level);
            int32_t mixed_r = (int32_t)(right * (0.7f + 0.3f * cos(surreal_phase))) + (int32_t)(noise * 500.0f * signal_level);
            
            if (mixed_l > 32767) mixed_l = 32767; else if (mixed_l < -32768) mixed_l = -32768;
            if (mixed_r > 32767) mixed_r = 32767; else if (mixed_r < -32768) mixed_r = -32768;
            
            out[i*2] = (int16_t)mixed_l;
            out[i*2+1] = (int16_t)mixed_r;
        }
    }

calc_amp:
    // Wenn gerade kein fester System-Beep spielt, nutze die Amplitude der Musik fuer die Visuals!
    if (linux_sound_duration == 0) {
        int64_t sum = 0;
        int num_samples = len / 2;
        for (int i = 0; i < num_samples; i++) {
            int16_t val = out[i];
            if (val < 0) val = -val;
            sum += val;
        }
        int avg_amp = sum / num_samples;
        
        if (avg_amp > 100) {
            linux_current_freq = avg_amp / 2; 
        } else {
            linux_current_freq = 0;
        }
    }
}
#include "boot_info.h"
#include <sys/mman.h>

SDL_AudioDeviceID global_audio_dev = 0;

int main(int argc, char** argv) {
    void* bare_metal_ram = mmap(NULL, 1024 * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (bare_metal_ram == MAP_FAILED) {
        printf("WARNING: Failed to allocate bare-metal 1GB RAM space!\n");
    }

    // SDL wird nun automatisch Pipewire/PulseAudio nutzen, da wir das in der ISO installiert haben!
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_DisableScreenSaver();

    SDL_Window* window = SDL_CreateWindow(
        "CosmOS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1920, 1080,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) return 1;

    // Allocate framebuffer memory for Cosmos OS to draw into
    fb = new uint32_t[1920 * 1080];

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    
    // Hide hardware cursor so only Cosmos OS Aero cursor is visible
    SDL_ShowCursor(SDL_DISABLE);
    if (!gl_ctx) return 1;
    
    SDL_GL_SetSwapInterval(0); // VSync OFF (Unlocks FPS and prevents 50% drops)

    // Init Audio in a background thread to prevent blocking the UI
    std::thread audio_init_thread([]() {
        sleep(2); // Wait for Pipewire to fully initialize before probing audio devices
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return;
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 2; // STEREO for Maximum Spread!
        want.samples = 2048;
        want.callback = audio_callback;
        SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (audio_dev > 0) SDL_PauseAudioDevice(audio_dev, 0);

        // Load Cat Sounds
        load_and_convert_wav("sounds/cat_meow.wav", 0, &have);
        load_and_convert_wav("sounds/nervous_cat.wav", 1, &have);
        load_and_convert_wav("sounds/cat_unhappy.wav", 2, &have);
        load_and_convert_wav("sounds/furious_cat.wav", 3, &have);
        
        global_audio_dev = audio_dev;
        while (true) sleep(1000); // Keep thread alive to prevent PulseAudio crash!
    });
    audio_init_thread.detach();

    // MOCK A HARD DRIVE FOR LINUX (Disk Manager)
    extern uint32_t drive_count;
    // Drive setup is now properly handled in ahci_mount_drive()

    // Boot Cosmos OS in a background thread!
    BootInfo boot_info = {0};
    boot_info.screen_width = 800;
    boot_info.screen_height = 600;
    boot_info.framebuffer_pitch = 800 * 4;
    
    std::thread cosmos_thread(cosmos_main, &boot_info);
    cosmos_thread.detach();

    // Create an OpenGL texture for the Cosmos OS framebuffer
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    bool running = true;
    uint32_t last_ticks_ms = SDL_GetTicks();
    while (running) {
        uint32_t frame_start = SDL_GetTicks();
        SDL_Event event;
        
        // --- Update system_ticks (1 tick per ms, matching bare-metal PIT at 1000 Hz) ---
        uint32_t now_ms = SDL_GetTicks();
        system_ticks += (now_ms - last_ticks_ms);
        os2_system_ticks += (now_ms - last_ticks_ms);
        last_ticks_ms = now_ms;
        
        int mx, my;
        uint32_t mb = SDL_GetMouseState(&mx, &my);
        
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        
        // Cosmos OS expects 800x600 (Desktop Mode)
        // Let's scale mouse coordinates
        float scale_x = (float)boot_info.screen_width / (float)w;
        float scale_y = (float)boot_info.screen_height / (float)h;
        
        linux_mx = (uint32_t)(mx * scale_x);
        linux_my = (uint32_t)(my * scale_y);
        
        if (mb & SDL_BUTTON(1)) {
            if (!linux_mdown) mouse_just_pressed = true;
            linux_mdown = true;
        } else {
            linux_mdown = false;
        }
        
        mouse_right_down = (mb & SDL_BUTTON(3)) != 0;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.scancode == SDL_SCANCODE_PRINTSCREEN) {
                    if (fork() == 0) { execlp("xfce4-screenshooter", "xfce4-screenshooter", "-f", NULL); exit(1); }
                }
                linux_scancode = sdl_to_ps2(event.key.keysym.sym);
            }
			if (event.type == SDL_MOUSEWHEEL) {
				mouse_wheel = event.wheel.y;
			}
            if (event.type == SDL_KEYUP) {
                uint8_t sc = sdl_to_ps2(event.key.keysym.sym);
                if (sc != 0) {
                    linux_scancode = sc | 0x80; // Release scancode
                }
            }
        }
        
        if (input_cooldown > 0) input_cooldown--;

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Upload the Cosmos OS Framebuffer to the texture
        if (fb != nullptr) {
            glViewport(0, 0, w, h);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            static uint32_t tex_w = 0;
            static uint32_t tex_h = 0;
            if (tex_w != boot_info.screen_width || tex_h != boot_info.screen_height) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, boot_info.screen_width, boot_info.screen_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
                tex_w = boot_info.screen_width;
                tex_h = boot_info.screen_height;
            }
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, boot_info.screen_width, boot_info.screen_height, GL_BGRA, GL_UNSIGNED_BYTE, fb);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, -1.0f);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
            glEnd();
            glDisable(GL_TEXTURE_2D);
        }
		if (app_window_active) {
            // 1. GANZ WICHTIG: Tiefenpuffer leeren!
            // Das sorgt dafür, dass die 3D-Katze räumlich VOR dem Cosmos-Desktop platziert wird.
            glClear(GL_DEPTH_BUFFER_BIT);

            // 2. 3D Engine im sicheren Main-Thread aufrufen!
            run_smash_cats_engine(os_app_x, os_app_y, os_app_w, os_app_h, false);

            // 3. Scissor-Test wieder ausmachen, damit der nächste Frame nicht kaputt geht
            glDisable(GL_SCISSOR_TEST);
        }

        // DEIN ALTER CODE:
        SDL_GL_SwapWindow(window);
        
        uint32_t frame_time = SDL_GetTicks() - frame_start;
        // FPS Cap bei ca. 950 FPS (1ms), um einen GPU/System-Absturz (Overkill > 1000 FPS) zu verhindern!
        if (frame_time < 1) {
            SDL_Delay(1);
        }
        
        // Failsafe: Wenn Audio nicht geladen werden konnte, simuliere den Ablauf der Ton-Dauer!
        if (global_audio_dev == 0 && linux_current_freq > 0) {
            if (linux_sound_duration != 0xFFFFFFFF) {
                if (linux_sound_duration > (44100 / 60)) linux_sound_duration -= (44100 / 60);
                else { linux_sound_duration = 0; linux_current_freq = 0; }
            }
        }
        
        frame++;
    }

    if (global_audio_dev > 0) SDL_CloseAudioDevice(global_audio_dev);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
