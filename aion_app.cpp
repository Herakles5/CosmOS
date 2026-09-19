#include "aion_app.h"
#include "json.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <algorithm>

#include "stb_image.h"
#include <thread>
#include <chrono>
#include <cctype>
#include <unistd.h>

using json = nlohmann::json;

extern void DrawRoundedRect(int x, int y, int w, int h, int radius, uint32_t color);
extern void Text(int x, int y, const char* text, uint32_t color, bool bold);
extern void PutPixelAlpha(uint32_t x, uint32_t y, uint32_t color, float alpha);
extern void PutPixel(uint32_t x, uint32_t y, uint32_t color);
extern int mouse_x;
extern int mouse_y;
extern int mouse_wheel;

static int face_width = 0;
static int face_height = 0;
static unsigned char* face_data = nullptr;

static std::vector<std::string> aion_history;
static std::string aion_response = "AION ONLINE.";
static bool is_thinking = false;
extern "C" { bool aion_is_thinking_exposed = false; }
extern "C" { bool aion_auto_listen = false; }
extern "C" { bool aion_wants_foreground = false; }
extern "C" { bool aion_wants_background = false; }
static int thinking_timer = 0;
static uint64_t speech_finish_timer = 0;
extern volatile uint64_t system_ticks;

extern "C" { int aion_lang = 1; } // 0=EN, 1=DE, 2=KO
extern "C" { bool aion_teach_mode = false; }
bool aion_ollama_online = false;

char aion_system_context[256] = "Active Window: Desktop";
volatile int aion_requested_app = -1;
static std::string current_aion_emotion = "CALM";
static int aion_detected_lang = 0;

static std::string pending_whisper_text = "";
static volatile bool has_pending_whisper = false;

static std::string pending_aion_response = "";
static volatile bool has_pending_aion_response = false;
static int aion_scroll = 0;

struct Drop { float y; float speed; };
static std::vector<std::vector<Drop>> columnDrops;
static std::vector<std::vector<char>> gridChars;

// Original Settings
const unsigned int FaceW = 640;
const unsigned int FaceH = 480;
const float FONT_SIZE = 1.0f;
static const int COLS = (int)(FaceW / FONT_SIZE);
static const int ROWS = (int)(FaceH / FONT_SIZE);
static const std::string symbols = ".o00´";

void InitAionApp() {
    if (!face_data) {
        int comp;
        face_data = stbi_load("face.png", &face_width, &face_height, &comp, 4);
        if (!face_data) face_data = stbi_load("/opt/meinos/face.png", &face_width, &face_height, &comp, 4);
        if (!face_data) face_data = stbi_load("/root/coding/MeinOS/face.png", &face_width, &face_height, &comp, 4);
    }
    
    columnDrops.resize(COLS);
    gridChars.resize(COLS);
    for(int x = 0; x < COLS; x++) {
        gridChars[x].resize(ROWS);
        columnDrops[x].clear();
        for(int i = 0; i < 30; i++) {
            Drop d;
            d.y = rand() % FaceH;
            d.speed = (rand() % 1) + 1.0f;
            columnDrops[x].push_back(d);
        }
        for(int y = 0; y < ROWS; y++) {
            gridChars[x][y] = symbols[rand() % symbols.size()];
        }
    }
}

void UpdateAionBackground() {
    static int ollama_ping_timer = 0;
    ollama_ping_timer++;
    if (ollama_ping_timer > 60) {
        ollama_ping_timer = 0;
        system("(curl -s --max-time 1 http://127.0.0.1:11434/ > /dev/null && echo 1 > /tmp/ollama_online.tmp || echo 0 > /tmp/ollama_online.tmp) &");
        
        FILE* ping_f = fopen("/tmp/ollama_online.tmp", "r");
        if (ping_f) {
            char st = fgetc(ping_f);
            aion_ollama_online = (st == '1');
            fclose(ping_f);
        }
    }

    if (has_pending_whisper) {
        has_pending_whisper = false;
        AionHandleInput(pending_whisper_text.c_str());
    }
    
    FILE* ide_f = fopen("/tmp/aion_ide_out.txt", "rb");
    if (ide_f) {
        fseek(ide_f, 0, SEEK_END);
        long sz = ftell(ide_f);
        fseek(ide_f, 0, SEEK_SET);
        if (sz > 0) {
            char* buf = new char[sz + 1];
            size_t read_bytes = fread(buf, 1, sz, ide_f);
            buf[read_bytes] = 0;
            AionHandleInput(buf);
            delete[] buf;
        }
        fclose(ide_f);
        system("rm -f /tmp/aion_ide_out.txt");
    }
    
    FILE* direct_f = fopen("/tmp/aion_direct.txt", "rb");
    if (direct_f) {
        fseek(direct_f, 0, SEEK_END);
        long sz = ftell(direct_f);
        fseek(direct_f, 0, SEEK_SET);
        if (sz > 0) {
            char* buf = new char[sz + 1];
            size_t read_bytes = fread(buf, 1, sz, direct_f);
            buf[read_bytes] = 0;
            
            // Bypass Ollama, add directly to history
            std::string direct_msg = buf;
            extern char aion_prediction[256];
            
            std::string pred_str = "AION: ";
            int pred_line_len = 6;
            for (char c : direct_msg) {
                if (c == '\n' || pred_line_len >= 50) {
                    pred_str += '\n';
                    pred_line_len = 0;
                    if (c == '\n') continue;
                }
                pred_str += c;
                pred_line_len++;
            }
            strncpy(aion_prediction, pred_str.c_str(), 255);
            aion_prediction[255] = 0;
            
            std::string current_line = "> AION: ";
            for (char c : direct_msg) {
                if (c == '\n' || current_line.length() >= 70) {
                    aion_history.push_back(current_line);
                    current_line = "  ";
                    if (c == '\n') continue;
                }
                current_line += c;
            }
            if (current_line.length() > 2) {
                aion_history.push_back(current_line);
            }
            
            while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
            
            extern void hs_add_log(const char* msg);
            hs_add_log(("AION: " + direct_msg).c_str());
            
            is_thinking = false;
            thinking_timer = 0;
            aion_wants_foreground = true;
            delete[] buf;
        }
        fclose(direct_f);
        system("rm -f /tmp/aion_direct.txt");
    }
    
    
    if (has_pending_aion_response) {
        has_pending_aion_response = false;
        
        std::string current_line = "";
        extern void hs_add_log(const char* msg);
        
        std::string log_prefix = "AION: ";
        for (char c : pending_aion_response) {
            if (c == '\n' || current_line.length() >= 75) {
                aion_history.push_back(current_line);
                std::string l = log_prefix + current_line;
                hs_add_log(l.c_str());
                current_line = "";
                if (c == '\n') continue;
            }
            current_line += c;
        }
        if (!current_line.empty()) {
            aion_history.push_back(current_line);
            std::string l = log_prefix + current_line;
            hs_add_log(l.c_str());
        }
        while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
        
        FILE* sf = fopen("/tmp/aion_speech.txt", "w");
        if (sf) {
            fwrite(pending_aion_response.c_str(), 1, pending_aion_response.length(), sf);
            fclose(sf);
        }
        
        FILE* sfi = fopen("/tmp/aion_to_ide.txt", "w");
        if (sfi) {
            fwrite(pending_aion_response.c_str(), 1, pending_aion_response.length(), sfi);
            fclose(sfi);
        }
        
        system("rm -f /tmp/aion_speech_done");
        aion_wants_foreground = true;
        
        std::string piper_model = "/root/coding/AI_Desktop/voice.onnx";
        if (aion_detected_lang == 1 || aion_teach_mode) piper_model = "/root/coding/AI_Desktop/de_DE-ramona-low.onnx";
        std::string piper_cmd = "(nice -n 19 /root/coding/AI_Desktop/piper/piper --model " + piper_model + " --output_file /tmp/aion_response.wav < /tmp/aion_speech.txt && pw-play /tmp/aion_response.wav; touch /tmp/aion_speech_done) &";
        system(piper_cmd.c_str());
        
        is_thinking = false;
        thinking_timer = 0;
        aion_scroll = 0;
    }
    
    FILE* check_f = fopen("/tmp/aion_speech_done", "rb");
    if (check_f) {
        fclose(check_f);
        system("rm -f /tmp/aion_speech_done");
        speech_finish_timer = system_ticks;
    }
    
    if (speech_finish_timer > 0 && (system_ticks - speech_finish_timer) > 5000) {
        speech_finish_timer = 0;
        aion_wants_background = true;
    }

    // API Polling moved here
    aion_is_thinking_exposed = is_thinking;
    if (is_thinking) {
        thinking_timer++;
        if (thinking_timer > 7200) { // 120 seconds timeout
            is_thinking = false;
            aion_is_thinking_exposed = false;
            thinking_timer = 0;
            aion_history.push_back("> [Error: API Timeout]");
            while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
            system("rm -f /tmp/aion.json /tmp/aion_tmp.json");
        }
        
        FILE* f = fopen("/tmp/aion.json", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                char* buf = new char[sz + 1];
                size_t read_bytes = fread(buf, 1, sz, f);
                buf[read_bytes] = 0;
                fclose(f);
                system("rm -f /tmp/aion.json");
                
                try {
                    json j = json::parse(buf);
                    if (j.contains("response")) {
                        std::string raw_resp = j["response"].get<std::string>();
                        std::thread([raw_resp]() {
                            std::string final_response = raw_resp;
                            size_t cmd_start = final_response.find("[CMD:");
                            if (cmd_start != std::string::npos) {
                                size_t cmd_end = final_response.find("]", cmd_start);
                                if (cmd_end != std::string::npos) {
                                    std::string cmd_str = final_response.substr(cmd_start + 5, cmd_end - (cmd_start + 5));
                                    size_t first_char = cmd_str.find_first_not_of(" \t");
                                    if (first_char != std::string::npos) cmd_str = cmd_str.substr(first_char);
                                    
                                    // SECURITY: Block dangerous commands
                                    bool blocked = false;
                                    std::string lower_cmd = cmd_str;
                                    std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
                                    if (lower_cmd.find("reboot") != std::string::npos || 
                                        lower_cmd.find("shutdown") != std::string::npos ||
                                        lower_cmd.find("kill") != std::string::npos ||
                                        lower_cmd.find("rm ") != std::string::npos ||
                                        lower_cmd.find("mkfs") != std::string::npos ||
                                        lower_cmd.find("dd ") != std::string::npos ||
                                        lower_cmd.find("halt") != std::string::npos) {
                                        blocked = true;
                                    }

                                    std::string result = "";
                                    if (!blocked) {
                                        FILE* pipe = popen(("timeout 5 " + cmd_str).c_str(), "r");
                                        if (pipe) {
                                            char pbuf[128];
                                            while (!feof(pipe)) {
                                                if (fgets(pbuf, 128, pipe) != NULL) result += pbuf;
                                            }
                                            pclose(pipe);
                                        }
                                    } else {
                                        result = "Command blocked due to missing permissions.";
                                    }
                                        std::replace(result.begin(), result.end(), '\n', ' ');
                                        std::replace(result.begin(), result.end(), '\r', ' ');
                                        
                                        std::string followup_prompt = "I ran your command. The result is: " + result + ". Please provide the final spoken answer to the user now, without any commands.";
                                        size_t pos = 0;
                                        while((pos = followup_prompt.find("\"", pos)) != std::string::npos) {
                                            followup_prompt.replace(pos, 1, "\\\"");
                                            pos += 2;
                                        }
                                        
                                        std::string json_body = "{\"model\":\"llama3.2\",\"prompt\":\"" + followup_prompt + "\",\"stream\":false}";
                                        FILE* req2 = fopen("/tmp/aion_req2.json", "w");
                                        if (req2) {
                                            fwrite(json_body.c_str(), 1, json_body.length(), req2);
                                            fclose(req2);
                                        }
                                        
                                        system("rm -f /tmp/aion_followup.json /tmp/aion_followup_tmp.json");
                                        system("(curl -s --max-time 120 -X POST http://127.0.0.1:11434/api/generate -d @/tmp/aion_req2.json > /tmp/aion_followup_tmp.json && mv /tmp/aion_followup_tmp.json /tmp/aion_followup.json) >/dev/null 2>&1 &");
                                        
                                        int timeout = 0;
                                        while (timeout < 1200) { // 120 seconds
                                            FILE* check = fopen("/tmp/aion_followup.json", "rb");
                                            if (check) {
                                                fclose(check);
                                                break;
                                            }
                                            usleep(100000); // 100ms
                                            timeout++;
                                        }
                                        
                                        FILE* f2 = fopen("/tmp/aion_followup.json", "rb");
                                        if (f2) {
                                            fseek(f2, 0, SEEK_END);
                                            long sz = ftell(f2);
                                            fseek(f2, 0, SEEK_SET);
                                            if (sz > 0) {
                                                char* buf2 = new char[sz + 1];
                                                size_t rb = fread(buf2, 1, sz, f2);
                                                buf2[rb] = 0;
                                                try {
                                                    json j2 = json::parse(buf2);
                                                    if (j2.contains("response")) {
                                                        final_response = j2["response"].get<std::string>();
                                                    }
                                                } catch(...) {}
                                                delete[] buf2;
                                            }
                                            fclose(f2);
                                        }
                                    }
                                }
                            
                            // Parse [EMOTION: xxx]
                            size_t emo_start = final_response.find("[EMOTION:");
                            if (emo_start != std::string::npos) {
                                size_t emo_end = final_response.find("]", emo_start);
                                if (emo_end != std::string::npos) {
                                    current_aion_emotion = final_response.substr(emo_start + 9, emo_end - (emo_start + 9));
                                    size_t first_char = current_aion_emotion.find_first_not_of(" \t");
                                    if (first_char != std::string::npos) current_aion_emotion = current_aion_emotion.substr(first_char);
                                    
                                    final_response.erase(emo_start, emo_end - emo_start + 1);
                                }
                            } else {
                                current_aion_emotion = "CALM";
                            }
                            
                            // Parse [SYS: OPEN, xxx]
                            size_t sys_start = final_response.find("[SYS: OPEN,");
                            if (sys_start != std::string::npos) {
                                size_t sys_end = final_response.find("]", sys_start);
                                if (sys_end != std::string::npos) {
                                    std::string app_name = final_response.substr(sys_start + 11, sys_end - (sys_start + 11));
                                    if (app_name.find("EARTHQUAKE") != std::string::npos) aion_requested_app = 17;
                                    else if (app_name.find("DEFENDER") != std::string::npos) aion_requested_app = 10;
                                    else if (app_name.find("SMASH") != std::string::npos) aion_requested_app = 14;
                                    else if (app_name.find("EMULATOR") != std::string::npos) aion_requested_app = 13;
                                    
                                    final_response.erase(sys_start, sys_end - sys_start + 1);
                                }
                            }
                            
                            size_t lang_en = final_response.find("[LANG: EN]");
                            size_t lang_de = final_response.find("[LANG: DE]");
                            if (lang_en != std::string::npos) {
                                aion_detected_lang = 0;
                                final_response.erase(lang_en, 10);
                            } else if (lang_de != std::string::npos) {
                                aion_detected_lang = 1;
                                final_response.erase(lang_de, 10);
                            }

                            pending_aion_response = final_response;
                            has_pending_aion_response = true;
                        }).detach();
                    } else if (j.contains("error")) {
                        pending_aion_response = "API ERROR: " + j["error"].get<std::string>();
                        has_pending_aion_response = true;
                    } else {
                        pending_aion_response = "API ERROR: Unexpected JSON format received.";
                        has_pending_aion_response = true;
                    }
                } catch(...) {
                    pending_aion_response = "Error parsing AI thought. Ollama might be offline.";
                    has_pending_aion_response = true;
                }
                
                delete[] buf;
            } else {
                fclose(f);
                system("rm -f /tmp/aion.json");
                aion_history.push_back("> [Error: API Connection Failed]");
                while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
                is_thinking = false;
                thinking_timer = 0;
            }
        }
    }
}

void UpdateAionApp(int cx, int cy, int cw, int ch, bool is_active) {
    int title_bar = 30;
    cy += title_bar;
    ch -= title_bar;
    
    DrawRoundedRect(cx, cy, cw, ch, 0, 0x000000); 
    
    static float anim_time = 0;
    anim_time += 0.05f;
    
    uint32_t stateBaseColor = 0x00FF64; // CALM
    if (current_aion_emotion == "ALERT") stateBaseColor = 0xFF1111;
    else if (current_aion_emotion == "HAPPY") stateBaseColor = 0xFFDD22;
    else if (current_aion_emotion == "THINKING") stateBaseColor = 0x3296FF;
    
    if (is_thinking) stateBaseColor = 0x3296FF; 
    
    static float nextBlinkTime = (float)(rand() % 3 + 2);
    static float blinkTimeElapsed = 0;
    static bool isBlinking = false;
    blinkTimeElapsed += 0.016f; 
    
    if (blinkTimeElapsed > nextBlinkTime) {
        isBlinking = true;
        if (blinkTimeElapsed > nextBlinkTime + 0.20f) {
            isBlinking = false; 
            blinkTimeElapsed = 0; 
            nextBlinkTime = (float)(rand() % 4 + 2);
        }
    }
    
    float scaleX = (float)cw / FaceW;
    float scaleY = (float)(ch - 210) / FaceH;
    
    extern uint8_t aion_use_3d_avatar;
    if (!aion_use_3d_avatar) {
        for (int x = 0; x < COLS; x++) {
            for (auto& d : columnDrops[x]) {
            d.y += d.speed * 0.65f;
            if (d.y > FaceH + 5) d.y = -5;
        }
        for (int y = 0; y < ROWS; y++) {
            float pX = x * FONT_SIZE;
            float pY = y * FONT_SIZE;
            
            unsigned int mX = (unsigned int)((pX / FaceW) * face_width);
            unsigned int mY = (unsigned int)((pY / FaceH) * face_height);
            
            if (mX >= (unsigned int)face_width || mY >= (unsigned int)face_height) continue;
            
            int final_px = cx + (int)(pX * scaleX);
            int final_py = cy + (int)(pY * scaleY);
            
            if (!face_data) {
                uint32_t c = (y > columnDrops[x].back().y - 3) ? 0x00FF00 : 0x004400;
                Text(final_px, final_py, std::string(1, gridChars[x][y]).c_str(), c, false);
                continue;
            }
            
            int pIdx = (mY * face_width + mX) * 4;
            int r = face_data[pIdx];
            int g = face_data[pIdx+1];
            int b = face_data[pIdx+2];
            int brightness = (r + g + b) / 4;

            char c = gridChars[x][y];
            float charAlpha = 1.0f;
            if (c == '.') charAlpha = 0.25f;
            else if (c == 'o') charAlpha = 0.65f;
            else if (c == '´') charAlpha = 0.40f;
            else if (c == '0') charAlpha = 0.95f;
            
            if (brightness > 40 && brightness < 60) {
                PutPixelAlpha(final_px, final_py, 0x2D2D2D, charAlpha);
                if (brightness < 50) continue; 
            }
            if (brightness < 80) continue;
            
            float baseInt = 0.77f;
            
            float dGlow = 0.11f;
            for (const auto& d : columnDrops[x]) {
                float dist = d.y - pY;
                if (dist > 0 && dist < 80) dGlow = std::max(dGlow, (1.0f - (dist / 80.0f)));
            }
            float highlight = baseInt + dGlow;
            
            int fr = (int)(((stateBaseColor >> 16) & 0xFF) * highlight);
            int fg = (int)(((stateBaseColor >> 8) & 0xFF) * highlight);
            int fb = (int)((stateBaseColor & 0xFF) * highlight);
            fr = std::min(255, fr);
            fg = std::min(255, fg);
            fb = std::min(255, fb);
            
            if (dGlow > 0.9f) { fr = 255; fg = 255; fb = 255; }
            
            bool isEyeArea = (y > ROWS * 0.40f && y < ROWS * 0.45f) && 
                             ((x > COLS * 0.30f && x < COLS * 0.40f) || (x > COLS * 0.60f && x < COLS * 0.70f));
            if (isBlinking && isEyeArea) continue;
            
            float wave = std::sin(anim_time * 5.0f + (x * 0.32f)) * std::cos(anim_time * 6.0f + (y * 0.2f));
            if (wave > 1.32f) {
                 float intensity = 10.6f + (wave * 21.11f); 
                 fr = std::min(255, (int)(fr + (255 * intensity)));
                 fg = std::min(255, (int)(fg + (255 * intensity)));
                 fb = std::min(255, (int)(fb + (255 * intensity)));
            } else if (wave < -1.01f) {
                fr = (int)(fr * 0.4f);
                fg = (int)(fg * 0.5f);
                fb = (int)(fb * 0.6f);
            }
            
            uint32_t finalCol = (fr << 16) | (fg << 8) | fb;
            int block_w = (int)ceil(scaleX);
            int block_h = (int)ceil(scaleY);
            if (block_w < 1) block_w = 1;
            if (block_h < 1) block_h = 1;
            DrawRoundedRect(final_px, final_py, block_w, block_h, 0, finalCol);
        }
    }
    

    }
    
    int console_y = cy + ch - 210;
    DrawRoundedRect(cx, console_y, cw, 170, 0, 0x111111);
    
    if (mouse_x >= cx && mouse_x <= cx + cw && mouse_y >= console_y && mouse_y <= console_y + 170) {
        if (mouse_wheel != 0) {
            aion_scroll -= mouse_wheel * 3;
            mouse_wheel = 0;
        }
    }
    int max_lines = 12;
    int max_scroll = (int)aion_history.size() - max_lines;
    if (max_scroll < 0) max_scroll = 0;
    if (aion_scroll > max_scroll) aion_scroll = max_scroll;
    if (aion_scroll < 0) aion_scroll = 0;
    
    int start_i = (int)aion_history.size() - max_lines - aion_scroll;
    if (start_i < 0) start_i = 0;
    
    int draw_line = 0;
    for (size_t i = start_i; i < aion_history.size() - aion_scroll; i++) {
        if (i >= aion_history.size()) break;
        uint32_t col = (aion_history[i].length() > 0 && aion_history[i][0] == '>') ? 0x88FF88 : 0xCCCCCC;
        Text(cx + 10, console_y + 10 + draw_line * 12, aion_history[i].c_str(), col, false);
        draw_line++;
    }
    
    if (is_thinking) {
        if (!aion_ollama_online) {
            Text(cx + 10, console_y + 10 + draw_line * 12, "Waiting for Ollama connection...", 0xFF5555, true);
        } else {
            Text(cx + 10, console_y + 10 + draw_line * 12, "AION is thinking...", 0x55AAFF, true);
        }
    }
    
    std::string stat_str = aion_ollama_online ? "[ OLLAMA: ONLINE ]" : "[ OLLAMA: OFFLINE ]";
    uint32_t stat_col = aion_ollama_online ? 0x00FF00 : 0xFF5555;
    Text(cx + cw - 150, console_y + 10, stat_str.c_str(), stat_col, false);
}

void AionHandleInput(const char* text) {
    if (is_thinking) return;
    is_thinking = true;
    thinking_timer = 0;
    aion_response = "";
    
    std::string prompt = text;
    aion_history.push_back("> " + prompt);
    while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
    
    size_t pos = 0;
    while((pos = prompt.find("\"", pos)) != std::string::npos) {
        prompt.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    // Replace newlines and carriage returns with spaces
    std::replace(prompt.begin(), prompt.end(), '\n', ' ');
    std::replace(prompt.begin(), prompt.end(), '\r', ' ');
    
    std::string system_prompt = "You are AION, the AI core of MeinOS with full root access. "
                                "1. To run linux commands, output [CMD: <bash command>]. Do not ask the user to run it. Example: [CMD: curl -s 'wttr.in/?format=3']. "
                                "2. If you want to remember something about the user, run [CMD: echo 'Information' >> /root/aion_memory.txt]. "
                                "3. To open an app, output [SYS: OPEN, <APP_NAME>]. Apps: EARTHQUAKE, DEFENDER, SMASH, EMULATOR. "
                                "4. Always output an emotion tag at the end of your response like [EMOTION: HAPPY], [EMOTION: ALERT], or [EMOTION: CALM]. "
                                "5. AUTO-DETECT LANGUAGE: If the user writes in English, you MUST start your response with [LANG: EN] and reply in English. If the user writes in German, you MUST start your response with [LANG: DE] and reply in German.";
                                
    FILE* mem_f = fopen("/root/aion_memory.txt", "rb");
    if (mem_f) {
        fseek(mem_f, 0, SEEK_END);
        long sz = ftell(mem_f);
        fseek(mem_f, 0, SEEK_SET);
        if (sz > 0) {
            char* mbuf = new char[sz + 1];
            size_t rb = fread(mbuf, 1, sz, mem_f);
            mbuf[rb] = 0;
            system_prompt += " LONG TERM MEMORY FILE CONTENTS: ";
            system_prompt += mbuf;
            delete[] mbuf;
        }
        fclose(mem_f);
    }
    
    system_prompt += " CURRENT SYSTEM CONTEXT: ";
    system_prompt += aion_system_context;
    
    std::replace(system_prompt.begin(), system_prompt.end(), '\n', ' ');
    std::replace(system_prompt.begin(), system_prompt.end(), '\r', ' ');
    
    std::string json_body = "{\"model\":\"llama3.2\",\"system\":\"" + system_prompt + "\",\"prompt\":\"" + prompt + "\",\"stream\":false}";
    
    system("rm -f /tmp/aion.json /tmp/aion_request.json");
    FILE* req = fopen("/tmp/aion_request.json", "w");
    if (req) {
        fwrite(json_body.c_str(), 1, json_body.length(), req);
        fclose(req);
    }
    
    std::string cmd = "(while ! curl -s --max-time 1 http://127.0.0.1:11434/ > /dev/null; do sleep 1; done; curl -s --max-time 120 -X POST http://127.0.0.1:11434/api/generate -d @/tmp/aion_request.json > /tmp/aion_tmp.json && mv /tmp/aion_tmp.json /tmp/aion.json) >/dev/null 2>&1 &";
    system(cmd.c_str());
}

void AionProactiveSpeak(const char* message) {
    if (is_thinking) return;
    is_thinking = true;
    thinking_timer = 0;
    aion_response = "";
    
    current_aion_emotion = "ALERT";
    
    std::string prompt = "SYSTEM EVENT TRIGGERED: ";
    prompt += message;
    prompt += " Please give a short, urgent verbal warning to the user now in 1 or 2 sentences.";
    if (aion_lang == 1 || aion_teach_mode) prompt += " Reply in German.";
    else if (aion_lang == 2) prompt += " Reply in Korean.";
    
    aion_history.push_back("> [SYSTEM ALERT TRIGGERED]");
    while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
    
    size_t pos = 0;
    while((pos = prompt.find("\"", pos)) != std::string::npos) {
        prompt.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    std::string json_body = "{\"model\":\"llama3.2\",\"system\":\"You are AION. Output [EMOTION: ALERT] at the end.\",\"prompt\":\"" + prompt + "\",\"stream\":false}";
    
    system("rm -f /tmp/aion.json /tmp/aion_request.json");
    FILE* req = fopen("/tmp/aion_request.json", "w");
    if (req) {
        fwrite(json_body.c_str(), 1, json_body.length(), req);
        fclose(req);
    }
    
    std::string cmd = "(while ! curl -s --max-time 1 http://127.0.0.1:11434/ > /dev/null; do sleep 1; done; curl -s --max-time 120 -X POST http://127.0.0.1:11434/api/generate -d @/tmp/aion_request.json > /tmp/aion_tmp.json && mv /tmp/aion_tmp.json /tmp/aion.json) >/dev/null 2>&1 &";
    system(cmd.c_str());
}

extern "C" void linux_start_mic_record();
extern "C" void linux_stop_mic_record();

extern "C" void AionToggleMic() {
    static bool is_recording = false;
    
    if (is_thinking) return;
    
    if (is_recording) {
        linux_stop_mic_record();
        
        is_recording = false;
        aion_auto_listen = false;
        aion_history.push_back("> [Transcribing audio...]");
        while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
        
        is_thinking = true;
        std::thread([]() {
            system("nice -n 19 ffmpeg -y -f s16le -ar 44100 -ac 2 -i /tmp/input.raw -ar 16000 -ac 1 /tmp/input.wav > /dev/null 2>&1");
            system("nice -n 19 /root/coding/AI_Desktop/whisper-cli-linux -t 1 -m /root/coding/AI_Desktop/ggml-base.bin -f /tmp/input.wav -l auto -otxt -of /tmp/whisper_out > /dev/null 2>&1");
            FILE* f = fopen("/tmp/whisper_out.txt", "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (sz > 0) {
                    char* buf = new char[sz + 1];
                    size_t bytes = fread(buf, 1, sz, f);
                    buf[bytes] = 0;
                    std::string text = buf;
                    delete[] buf;
                    
                    size_t first_bracket = text.find("[");
                    while(first_bracket != std::string::npos) {
                        size_t end_bracket = text.find("]", first_bracket);
                        if (end_bracket != std::string::npos) text.erase(first_bracket, end_bracket - first_bracket + 1);
                        else break;
                        first_bracket = text.find("[");
                    }
                    
                    std::string trimmed = text;
                    size_t start = trimmed.find_first_not_of(" \t\n\r[]()");
                    if (start != std::string::npos) {
                        size_t end = trimmed.find_last_not_of(" \t\n\r[]()");
                        trimmed = trimmed.substr(start, end - start + 1);
                    } else {
                        trimmed = "";
                    }
                    
                    if (trimmed.length() > 2) {
                        pending_whisper_text = trimmed;
                        is_thinking = false;
                        has_pending_whisper = true;
                    } else {
                        is_thinking = false;
                    }
                } else {
                    is_thinking = false;
                }
                fclose(f);
            } else {
                is_thinking = false;
            }
        }).detach();
    } else {
        is_recording = true;
        aion_auto_listen = true;
        linux_start_mic_record();
        aion_history.push_back("> [Recording... click AUTO again to stop]");
        while (aion_history.size() > 1000) aion_history.erase(aion_history.begin());
    }
}
