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
static uint8_t sdl_to_ps2(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_ESCAPE: return 0x01;
        case SDL_SCANCODE_1: return 0x02; case SDL_SCANCODE_2: return 0x03; case SDL_SCANCODE_3: return 0x04;
        case SDL_SCANCODE_4: return 0x05; case SDL_SCANCODE_5: return 0x06; case SDL_SCANCODE_6: return 0x07;
        case SDL_SCANCODE_7: return 0x08; case SDL_SCANCODE_8: return 0x09; case SDL_SCANCODE_9: return 0x0A;
        case SDL_SCANCODE_0: return 0x0B;
        case SDL_SCANCODE_MINUS: return 0x0C; case SDL_SCANCODE_EQUALS: return 0x0D;
        case SDL_SCANCODE_BACKSPACE: return 0x0E; case SDL_SCANCODE_TAB: return 0x0F;
        case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11; case SDL_SCANCODE_E: return 0x12;
        case SDL_SCANCODE_R: return 0x13; case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Z: return 0x15;
        case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17; case SDL_SCANCODE_O: return 0x18;
        case SDL_SCANCODE_P: return 0x19;
        case SDL_SCANCODE_RETURN: return 0x1C;
        case SDL_SCANCODE_A: return 0x1E; case SDL_SCANCODE_S: return 0x1F; case SDL_SCANCODE_D: return 0x20;
        case SDL_SCANCODE_F: return 0x21; case SDL_SCANCODE_G: return 0x22; case SDL_SCANCODE_H: return 0x23;
        case SDL_SCANCODE_J: return 0x24; case SDL_SCANCODE_K: return 0x25; case SDL_SCANCODE_L: return 0x26;
        case SDL_SCANCODE_Y: return 0x2C; case SDL_SCANCODE_X: return 0x2D; case SDL_SCANCODE_C: return 0x2E;
        case SDL_SCANCODE_V: return 0x2F; case SDL_SCANCODE_B: return 0x30; case SDL_SCANCODE_N: return 0x31;
        case SDL_SCANCODE_M: return 0x32;
        case SDL_SCANCODE_COMMA: return 0x33; case SDL_SCANCODE_PERIOD: return 0x34;
        case SDL_SCANCODE_SLASH: return 0x35;
        case SDL_SCANCODE_SPACE: return 0x39;
        case SDL_SCANCODE_F1: return 0x3B; case SDL_SCANCODE_F2: return 0x3C; case SDL_SCANCODE_F3: return 0x3D;
        case SDL_SCANCODE_F4: return 0x3E; case SDL_SCANCODE_F5: return 0x3F; case SDL_SCANCODE_F6: return 0x40;
        case SDL_SCANCODE_F7: return 0x41; case SDL_SCANCODE_F8: return 0x42; case SDL_SCANCODE_F9: return 0x43;
        case SDL_SCANCODE_F10: return 0x44; case SDL_SCANCODE_F11: return 0x85; case SDL_SCANCODE_F12: return 0x86;
        case SDL_SCANCODE_UP: return 0x48; case SDL_SCANCODE_DOWN: return 0x50;
        case SDL_SCANCODE_LEFT: return 0x4B; case SDL_SCANCODE_RIGHT: return 0x4D;
        case SDL_SCANCODE_LSHIFT: return 0x2A; case SDL_SCANCODE_RSHIFT: return 0x36;
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

#include <mutex>
std::mutex linux_capture_mutex;
int16_t linux_capture_buffer[44100 * 2 * 2]; // 2 seconds of stereo
int linux_capture_head = 0;
int linux_capture_tail = 0;
int linux_capture_count = 0;

void audio_capture_callback(void* userdata, Uint8* stream, int len) {
    int16_t* in = (int16_t*)stream;
    int samples = len / 2;
    std::lock_guard<std::mutex> lock(linux_capture_mutex);
    static float env = 0.0f;
    for (int i = 0; i < samples; i++) {
        int16_t val = in[i];
        float abs_val = abs(val);
        // Envelope follower: Fast attack, slow release
        if (abs_val > env) env = env + 0.1f * (abs_val - env);
        else env = env + 0.001f * (abs_val - env);
        

        if (linux_capture_count < 44100 * 2 * 2) {
            linux_capture_buffer[linux_capture_head] = val;
            linux_capture_head = (linux_capture_head + 1) % (44100 * 2 * 2);
            linux_capture_count++;
        }
    }
}

// Audio Callback
void audio_callback(void* userdata, Uint8* stream, int len) {
    static float phase_left = 0.0f;
    static float phase_right = 0.0f;
    int16_t* out = (int16_t*)stream;
    int num_frames = len / 4; // 16-bit stereo = 4 bytes per frame
    
    // Initialize Stream to 0 to prepare for mixing
    memset(stream, 0, len);
    
    // Read captured desktop audio ONLY for visualizer amplitude (do not mix to output to prevent feedback/hiss)
    int64_t capture_sum = 0;
    int64_t capture_high_sum = 0;
    int capture_samples_read = 0;
    {
        std::lock_guard<std::mutex> lock(linux_capture_mutex);
        int samples = len / 2;
        int to_read = (linux_capture_count < samples) ? linux_capture_count : samples;
        int16_t prev_val = 0;
        for (int i = 0; i < to_read; i++) {
            int16_t val = linux_capture_buffer[linux_capture_tail];
            
            // High frequency tracking (difference between adjacent samples)
            int32_t diff = (int32_t)val - (int32_t)prev_val;
            if (diff < 0) diff = -diff;
            capture_high_sum += diff;
            prev_val = val;

            if (val < 0) val = -val;
            capture_sum += val;
            linux_capture_tail = (linux_capture_tail + 1) % (44100 * 2 * 2);
            linux_capture_count--;
            capture_samples_read++;
        }
    }

    if (linux_wav_len > 0 && linux_wav_pos < linux_wav_len) {
        int bytes_to_copy = len;
        if (linux_wav_pos + bytes_to_copy > linux_wav_len) {
            bytes_to_copy = linux_wav_len - linux_wav_pos;
        }
        
        // Additive mix for bare metal audio
        int16_t* wav_src = (int16_t*)(linux_wav_buffer + linux_wav_pos);
        for(int i = 0; i < bytes_to_copy / 2; i++) {
            int32_t mixed = out[i] + wav_src[i];
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;
            out[i] = (int16_t)mixed;
        }
        
        linux_wav_pos += bytes_to_copy;

        if (bytes_to_copy < len) {
            linux_wav_len = 0;
        }
        goto apply_surreal;
    }
    
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
            // This prevents any residual noise/hiss from playing during silence
            if (abs(left) < 50 && abs(right) < 50) continue;

            surreal_phase += 0.005f;
            if (surreal_phase > 6.28318f) surreal_phase -= 6.28318f;

            float mist = sin(surreal_phase);

            // Apply alien/surreal ring modulation (without white noise)
            int32_t mixed_l = (int32_t)(left * (0.7f + 0.3f * mist));
            int32_t mixed_r = (int32_t)(right * (0.7f + 0.3f * cos(surreal_phase)));
            
            if (mixed_l > 32767) mixed_l = 32767; else if (mixed_l < -32768) mixed_l = -32768;
            if (mixed_r > 32767) mixed_r = 32767; else if (mixed_r < -32768) mixed_r = -32768;
            
            out[i*2] = (int16_t)mixed_l;
            out[i*2+1] = (int16_t)mixed_r;
        }
    }

calc_amp:
    // Wenn gerade kein fester System-Beep spielt, nutze die Amplitude der Musik fuer die Visuals!
    if (linux_sound_duration == 0) {
        int32_t avg_amp = 0;
        int32_t avg_high_amp = 0;
        if (capture_samples_read > 0) {
            avg_amp = capture_sum / capture_samples_read;
            avg_high_amp = capture_high_sum / capture_samples_read;
        } else {
            // Fallback
            int64_t sum = 0;
            int64_t sum_high = 0;
            int num_samples = len / 4;
            int16_t prev_val = 0;
            for (int i=0; i<num_samples; i++) {
                int16_t val = (out[i*2] + out[i*2+1]) / 2;
                
                int32_t diff = (int32_t)val - (int32_t)prev_val;
                if (diff < 0) diff = -diff;
                sum_high += diff;
                prev_val = val;

                if (val < 0) val = -val;
                sum += val;
            }
            avg_amp = sum / num_samples;
            avg_high_amp = sum_high / num_samples;
        }
        
        // Combine low and high frequency amplitudes so all tones register
        avg_amp = avg_amp + (avg_high_amp / 4);

        // PURE AMPLITUDE DIAGRAM
        // No envelopes, no noise floors, no thresholds! 
        // This passes the raw volume directly to the sun, making it look exactly like a raw audio waveform!
        // Note: This requires the microphone to be UNPLUGGED (Stereo Mix), otherwise white noise will spike it.
        linux_current_freq = avg_amp / 2;
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
        
          int num_capture_devs = SDL_GetNumAudioDevices(1);
          printf("\n[SYS] Found %d Capture Devices:\n", num_capture_devs);
          for (int i = 0; i < num_capture_devs; i++) {
              printf("  -> %s\n", SDL_GetAudioDeviceName(i, 1));
          want.callback = audio_capture_callback;
          }
          SDL_AudioDeviceID capture_dev = SDL_OpenAudioDevice(NULL, 1, &want, &have, 0);
          if (capture_dev > 0) SDL_PauseAudioDevice(capture_dev, 0);
          if (capture_dev > 0) SDL_PauseAudioDevice(capture_dev, 0);
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
                linux_scancode = sdl_to_ps2(event.key.keysym.scancode);
            }
			if (event.type == SDL_MOUSEWHEEL) {
				mouse_wheel = event.wheel.y;
			}
            if (event.type == SDL_KEYUP) {
                uint8_t sc = sdl_to_ps2(event.key.keysym.scancode);
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
