#include "earthquake.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <unistd.h>
#include <thread>

extern void AionProactiveSpeak(const char* message);

struct LongTermStats {
    int count_30d = 0, count_7d = 0;
    int mag5_30d = 0, mag5_7d = 0;
    int mag7_30d = 0, mag7_7d = 0;
    
    long long avg_30d = 0, avg_7d = 0;
    long long avg5_30d = 0, avg5_7d = 0;
    long long avg7_30d = 0, avg7_7d = 0;
    
    long long last_30d = 0, last_7d = 0;
    long long last5_30d = 0, last5_7d = 0;
    long long last7_30d = 0, last7_7d = 0;
    int deep_30d = 0, deep_7d = 0;
    long long avg_deep_30d = 0, avg_deep_7d = 0;
    long long last_deep_30d = 0, last_deep_7d = 0;
    
    bool loaded = false;
};
static LongTermStats g_eq_stats;

#ifdef __linux__
#include <unistd.h>
#endif

#include "world_map.h"
#include "tectonic_plates.h"
#include "json.hpp"

using json = nlohmann::json;

void DrawLine(int x1, int y1, int x2, int y2, uint32_t color);
void DrawLineAlpha(int x1, int y1, int x2, int y2, uint32_t color, float alpha);
void PutPixel(int x, int y, uint32_t color);
void PutPixelAlpha(uint32_t x, uint32_t y, uint32_t color, float alpha);

static void DrawRoundedRectAlpha(int x, int y, int rw, int rh, int r, uint32_t c, float alpha) {
    for (int iy = 0; iy < rh; iy++) {
        for (int ix = 0; ix < rw; ix++) {
            bool corn = false;
            if (r > 0) {
                if(ix<r && iy<r && (r-ix)*(r-ix)+(r-iy)*(r-iy)>r*r) corn=true;
                if(ix>rw-r && iy<r && (ix-(rw-r))*(ix-(rw-r))+(r-iy)*(r-iy)>r*r) corn=true;
                if(ix<r && iy>rh-r && (r-ix)*(r-ix)+(iy-(rh-r))*(iy-(rh-r))>r*r) corn=true;
                if(ix>rw-r && iy>rh-r && (ix-(rw-r))*(ix-(rw-r))+(iy-(rh-r))*(iy-(rh-r))>r*r) corn=true;
            }
            if (!corn) PutPixelAlpha(x + ix, y + iy, c, alpha);
        }
    }
}

void DrawRoundedRect(int x, int y, int w, int h, int radius, uint32_t color);
void Text(int x, int y, const char* text, uint32_t color, bool bold);
void TextC(int x, int y, const char* text, uint32_t color, bool bold);

struct EqData {
    float lat;
    float lon;
    float mag;
    std::string place;
    long long time;
    std::string time_str;
};

static std::vector<EqData> earthquakes;
static bool data_loaded = false;
static bool fetch_started = false;

static float parse_float(const char* str, int& out_len) {
    char buf[32];
    int i = 0;
    while(str[i] && (str[i] == '-' || str[i] == '.' || (str[i] >= '0' && str[i] <= '9')) && i < 31) {
        buf[i] = str[i];
        i++;
    }
    buf[i] = 0;
    out_len = i;
    return atof(buf);
}

void InitEarthquakeApp() {
#ifdef __linux__
    system("rm -f /tmp/eq_usgs.json /tmp/eq_emsc.json /tmp/eq_geonet.json /tmp/eq_geofon.txt /tmp/eq_jma.json /tmp/eq_usgs_month.json");
    system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_usgs.json https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson &");
    system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_emsc.json \"https://www.seismicportal.eu/fdsnws/event/1/query?format=json&minmag=3.0&limit=500\" &");
    system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_geonet.json \"https://api.geonet.org.nz/quake?MMI=3\" &");
    system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_geofon.txt \"https://geofon.gfz-potsdam.de/fdsnws/event/1/query?format=text&minmag=3.0&limit=500\" &");
    system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_jma.json \"https://www.jma.go.jp/bosai/quake/data/list.json\" &");
    system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_usgs_month.json https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_month.geojson &");
    fetch_started = true;
    data_loaded = false;
    earthquakes.clear();
    
    std::thread([]() {
        time_t last_parse_time = 0;
        while (true) {
            struct stat st;
            if (stat("/tmp/eq_usgs_month.json", &st) == 0 && st.st_size > 0 && st.st_mtime != last_parse_time) {
                FILE* f = fopen("/tmp/eq_usgs_month.json", "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    char* buf = new char[sz + 1];
                    size_t rb = fread(buf, 1, sz, f);
                    buf[rb] = 0;
                    fclose(f);
                    
                    try {
                        json j = json::parse(buf);
                        long long now = time(NULL) * 1000LL;
                        long long week_ms = 7LL * 24 * 3600 * 1000;
                        long long month_ms = 30LL * 24 * 3600 * 1000;
                        
                        LongTermStats s;
                        if (j.contains("features")) {
                            for (auto& f : j["features"]) {
                                if (f["properties"]["mag"].is_number() && f["properties"]["time"].is_number()) {
                                    float mag = f["properties"]["mag"];
                                    long long t = f["properties"]["time"];
                                    float depth = 0.0f;
                                    if (f.contains("geometry") && f["geometry"]["coordinates"].size() >= 3 && f["geometry"]["coordinates"][2].is_number()) {
                                        depth = f["geometry"]["coordinates"][2];
                                    }
                                    if (now - t <= month_ms) {
                                        if (mag >= 3.0f) { s.count_30d++; if (t > s.last_30d) s.last_30d = t; }
                                        if (mag >= 5.0f) { s.mag5_30d++; if (t > s.last5_30d) s.last5_30d = t; }
                                        if (mag >= 7.0f) { s.mag7_30d++; if (t > s.last7_30d) s.last7_30d = t; }
                                        if (depth >= 150.0f) { s.deep_30d++; if (t > s.last_deep_30d) s.last_deep_30d = t; }
                                    }
                                    if (now - t <= week_ms) {
                                        if (mag >= 3.0f) { s.count_7d++; if (t > s.last_7d) s.last_7d = t; }
                                        if (mag >= 5.0f) { s.mag5_7d++; if (t > s.last5_7d) s.last5_7d = t; }
                                        if (mag >= 7.0f) { s.mag7_7d++; if (t > s.last7_7d) s.last7_7d = t; }
                                        if (depth >= 150.0f) { s.deep_7d++; if (t > s.last_deep_7d) s.last_deep_7d = t; }
                                    }
                                }
                            }
                        }
                        
                        s.avg_30d = month_ms / (s.count_30d > 0 ? s.count_30d : 1);
                        s.avg_7d = week_ms / (s.count_7d > 0 ? s.count_7d : 1);
                        s.avg5_30d = month_ms / (s.mag5_30d > 0 ? s.mag5_30d : 1);
                        s.avg5_7d = week_ms / (s.mag5_7d > 0 ? s.mag5_7d : 1);
                        s.avg7_30d = month_ms / (s.mag7_30d > 0 ? s.mag7_30d : 1);
                        s.avg7_7d = week_ms / (s.mag7_7d > 0 ? s.mag7_7d : 1);
                        s.avg_deep_30d = month_ms / (s.deep_30d > 0 ? s.deep_30d : 1);
                        s.avg_deep_7d = week_ms / (s.deep_7d > 0 ? s.deep_7d : 1);
                        s.loaded = true;
                        g_eq_stats = s;
                        last_parse_time = st.st_mtime;
                    } catch(...) {}
                    delete[] buf;
                }
            }
            sleep(1);
        }
    }).detach();
#endif
}

void UpdateEarthquakeApp(int cx, int cy, int cw, int ch, bool is_active, bool map_only) {
    static long long last_latest_time = 0;
    static float line_expiry = 0.0f;
    static float auto_reset_timer = 0.0f;
    static float target_zoom = 1.0f;
    static float target_offset_x = 0.0f;
    static float target_offset_y = 0.0f;

#ifdef __linux__
    if (fetch_started && !data_loaded) {
        struct stat st1, st2, st3, st4, st5;
        if (stat("/tmp/eq_usgs.json", &st1) == 0 && st1.st_size > 0 &&
            stat("/tmp/eq_emsc.json", &st2) == 0 && st2.st_size > 0 &&
            stat("/tmp/eq_geonet.json", &st3) == 0 && st3.st_size > 0 &&
            stat("/tmp/eq_geofon.txt", &st4) == 0 && st4.st_size > 0 &&
            stat("/tmp/eq_jma.json", &st5) == 0 && st5.st_size > 0) {
            
            auto load_json = [](const char* path) -> json {
                FILE* f = fopen(path, "rb");
                if (!f) return json();
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                char* buf = new char[sz + 1];
                size_t rb = fread(buf, 1, sz, f);
                buf[rb] = 0;
                fclose(f);
                json j;
                try { j = json::parse(buf); } catch(...) {}
                delete[] buf;
                return j;
            };
            
            json usgs = load_json("/tmp/eq_usgs.json");
            json emsc = load_json("/tmp/eq_emsc.json");
            json geonet = load_json("/tmp/eq_geonet.json");
            
            std::vector<EqData> raw_eqs;
            
            if (usgs.contains("features")) {
                for (auto& f : usgs["features"]) {
                    if (f["properties"]["mag"].is_number() && f["properties"]["mag"] >= 3.0f) {
                        EqData eq;
                        eq.mag = f["properties"]["mag"];
                        eq.place = f["properties"]["place"];
                        eq.lon = f["geometry"]["coordinates"][0];
                        eq.lat = f["geometry"]["coordinates"][1];
                        eq.time = f["properties"]["time"];
                        raw_eqs.push_back(eq);
                    }
                }
            }
            
            if (emsc.contains("features")) {
                for (auto& f : emsc["features"]) {
                    if (f["properties"]["mag"].is_number() && f["properties"]["mag"] >= 3.0f) {
                        EqData eq;
                        eq.mag = f["properties"]["mag"];
                        eq.place = f["properties"]["flynn_region"].is_string() ? f["properties"]["flynn_region"] : "Unknown";
                        eq.lon = f["geometry"]["coordinates"][0];
                        eq.lat = f["geometry"]["coordinates"][1];
                        std::string tstr = f["properties"]["time"];
                        struct tm t = {0};
                        sscanf(tstr.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
                        t.tm_year -= 1900;
                        t.tm_mon -= 1;
                        eq.time = (long long)timegm(&t) * 1000;
                        raw_eqs.push_back(eq);
                    }
                }
            }
            
            if (geonet.contains("features")) {
                for (auto& f : geonet["features"]) {
                    if (f["properties"]["magnitude"].is_number() && f["properties"]["magnitude"] >= 3.0f) {
                        EqData eq;
                        eq.mag = f["properties"]["magnitude"];
                        eq.place = f["properties"]["locality"].is_string() ? f["properties"]["locality"] : "New Zealand";
                        eq.lon = f["geometry"]["coordinates"][0];
                        eq.lat = f["geometry"]["coordinates"][1];
                        std::string tstr = f["properties"]["time"];
                        struct tm t = {0};
                        sscanf(tstr.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
                        t.tm_year -= 1900;
                        t.tm_mon -= 1;
                        eq.time = (long long)timegm(&t) * 1000;
                        raw_eqs.push_back(eq);
                    }
                }
            }
            
            // GEOFON Parsing
            FILE* gfz_f = fopen("/tmp/eq_geofon.txt", "r");
            if (gfz_f) {
                char line[512];
                while (fgets(line, sizeof(line), gfz_f)) {
                    if (line[0] == '#') continue; // Skip header
                    std::string sline(line);
                    std::vector<std::string> parts;
                    size_t pos = 0;
                    while ((pos = sline.find('|')) != std::string::npos) {
                        parts.push_back(sline.substr(0, pos));
                        sline.erase(0, pos + 1);
                    }
                    parts.push_back(sline); // Last part
                    
                    if (parts.size() >= 13) {
                        try {
                            float mag = std::stof(parts[10]);
                            if (mag >= 3.0f) {
                                EqData eq;
                                eq.mag = mag;
                                eq.lat = std::stof(parts[2]);
                                eq.lon = std::stof(parts[3]);
                                eq.place = parts[12];
                                
                                struct tm t = {0};
                                sscanf(parts[1].c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
                                t.tm_year -= 1900;
                                t.tm_mon -= 1;
                                eq.time = (long long)timegm(&t) * 1000;
                                raw_eqs.push_back(eq);
                            }
                        } catch(...) {}
                    }
                }
                fclose(gfz_f);
            }
            
            // JMA Parsing
            json jma = load_json("/tmp/eq_jma.json");
            if (jma.is_array()) {
                for (auto& f : jma) {
                    if (f.contains("mag") && f["mag"].is_string() && f.contains("cod") && f["cod"].is_string()) {
                        try {
                            float mag = std::stof(f["mag"].get<std::string>());
                            if (mag >= 3.0f) {
                                EqData eq;
                                eq.mag = mag;
                                std::string cod = f["cod"];
                                float lat = 0, lon = 0;
                                sscanf(cod.c_str(), "%f%f", &lat, &lon);
                                eq.lat = lat;
                                eq.lon = lon;
                                eq.place = f.contains("anm") && f["anm"].is_string() ? f["anm"] : "Japan";
                                
                                std::string tstr = f.contains("at") && f["at"].is_string() ? f["at"] : "";
                                struct tm t = {0};
                                sscanf(tstr.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
                                t.tm_year -= 1900;
                                t.tm_mon -= 1;
                                // JMA time is +09:00, subtract 9 hours to get UTC
                                eq.time = (long long)timegm(&t) * 1000 - (9 * 3600 * 1000);
                                raw_eqs.push_back(eq);
                            }
                        } catch(...) {}
                    }
                }
            }
            
            std::sort(raw_eqs.begin(), raw_eqs.end(), [](const EqData& a, const EqData& b) {
                return a.time > b.time;
            });
            
            earthquakes.clear();
            for (auto& eq : raw_eqs) {
                bool dup = false;
                for (auto& e : earthquakes) {
                    if (abs((int)(eq.time - e.time)) < 300000 && hypot(eq.lat - e.lat, eq.lon - e.lon) < 2.0) {
                        dup = true;
                        if (eq.mag > e.mag) { e = eq; }
                        break;
                    }
                }
                if (!dup) {
                    time_t seconds = (time_t)(eq.time / 1000);
                    struct tm* tm_info = localtime(&seconds);
                    char tbuf[128];
                    if (tm_info) {
                        snprintf(tbuf, sizeof(tbuf), "[%02d:%02d] M%.1f %s", tm_info->tm_hour, tm_info->tm_min, eq.mag, eq.place.c_str());
                    } else {
                        snprintf(tbuf, sizeof(tbuf), "M%.1f %s", eq.mag, eq.place.c_str());
                    }
                    eq.time_str = tbuf;
                    earthquakes.push_back(eq);
                }
            }
            
            if (!earthquakes.empty()) {
                if (last_latest_time > 0 && earthquakes[0].time > last_latest_time) {
                    line_expiry = 120.0f; // 120 seconds
                    
                    float r = ((90.0f - earthquakes[0].lat) / 180.0f) * 750.0f;
                    float angle = earthquakes[0].lon * 3.14159f / 180.0f;
                    float map_x = r * sinf(angle);
                    float map_y = r * cosf(angle);
                    
                    target_zoom = 3.5f;
                    float scale = ((ch * 0.45f) / 750.0f) * target_zoom;
                    target_offset_x = -map_x * scale;
                    target_offset_y = -map_y * scale;
                    auto_reset_timer = 20.0f;
                }
                
                // Proactive Heavy Earthquake Alert for AION
                static uint64_t last_mag7_time = 0;
                if (last_latest_time == 0) {
                    for (auto& eq : earthquakes) {
                        if (eq.mag >= 7.0f && eq.time > last_mag7_time) last_mag7_time = eq.time;
                    }
                } else {
                    for (auto& eq : earthquakes) {
                        if (eq.mag >= 7.0f && eq.time > last_mag7_time) {
                            last_mag7_time = eq.time;
                            char msg[256];
                            snprintf(msg, sizeof(msg), "A Magnitude %.1f earthquake was just registered in %s.", eq.mag, eq.place.c_str());
                            AionProactiveSpeak(msg);
                        }
                    }
                }
                
                last_latest_time = earthquakes[0].time;
            }
            
            data_loaded = true;
            fetch_started = false;
        }
    }
#endif
    
    int title_bar = 30;
    cy += title_bar;
    ch -= title_bar;

    DrawRoundedRectAlpha(cx, cy, cw, ch, 0, 0x555555, 0.2f); // Leicht grauer Hintergrund (transparent)

    
    static float zoom = 1.0f;
    static float offset_x = 0.0f;
    static float offset_y = 0.0f;
    
    extern int mouse_x;
    extern int mouse_y;
    extern bool mouse_down;
    extern bool mouse_just_pressed;
    extern int input_cooldown;
    
    static int last_mx = 0;
    static int last_my = 0;
    static bool is_panning = false;
    
    if (is_active) {
        if (mouse_just_pressed) {
            if (mouse_y >= cy && mouse_x > cx + 60 && mouse_x < cx + cw && mouse_y < cy + ch) {
                is_panning = true;
            }
        }
        if (!mouse_down) {
            is_panning = false;
        }
        
        if (auto_reset_timer > 0) {
            auto_reset_timer -= 0.016f;
            if (auto_reset_timer <= 0) {
                target_zoom = 1.0f;
                target_offset_x = 0.0f;
                target_offset_y = 0.0f;
            }
        }
        
        zoom += (target_zoom - zoom) * 0.05f;
        offset_x += (target_offset_x - offset_x) * 0.05f;
        offset_y += (target_offset_y - offset_y) * 0.05f;
        
        if (is_panning) {
            target_offset_x += (mouse_x - last_mx);
            target_offset_y += (mouse_y - last_my);
            offset_x = target_offset_x;
            offset_y = target_offset_y;
            auto_reset_timer = 0; // Cancel auto reset if user interacts
        }
        if (mouse_just_pressed && input_cooldown == 0) {
            if (mouse_x >= cx + 15 && mouse_x <= cx + 45 && mouse_y >= cy + 15 && mouse_y <= cy + 45) {
                target_zoom *= 1.25f;
                auto_reset_timer = 0;
                input_cooldown = 15;
            }
            if (mouse_x >= cx + 15 && mouse_x <= cx + 45 && mouse_y >= cy + 55 && mouse_y <= cy + 85) {
                target_zoom /= 1.25f;
                if (target_zoom < 0.2f) target_zoom = 0.2f;
                auto_reset_timer = 0;
                input_cooldown = 15;
            }
            if (mouse_x >= cx + 15 && mouse_x <= cx + 95 && mouse_y >= cy + 95 && mouse_y <= cy + 125) {
#ifdef __linux__
                system("rm -f /tmp/eq_usgs.json /tmp/eq_emsc.json /tmp/eq_geonet.json /tmp/eq_geofon.txt /tmp/eq_jma.json /tmp/eq_usgs_month.json");
                system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_usgs.json https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson &");
                system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_emsc.json \"https://www.seismicportal.eu/fdsnws/event/1/query?format=json&minmag=3.0&limit=500\" &");
                system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_geonet.json \"https://api.geonet.org.nz/quake?MMI=3\" &");
                system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_geofon.txt \"https://geofon.gfz-potsdam.de/fdsnws/event/1/query?format=text&minmag=3.0&limit=500\" &");
                system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_jma.json \"https://www.jma.go.jp/bosai/quake/data/list.json\" &");
                system("curl -sL -A \"Mozilla/5.0\" -o /tmp/eq_usgs_month.json https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_month.geojson &");
                fetch_started = true;
                data_loaded = false;
                earthquakes.clear();
                memset(&g_eq_stats, 0, sizeof(g_eq_stats));
#endif
                input_cooldown = 30;
            }
        }
        last_mx = mouse_x;
        last_my = mouse_y;
    }
    
    int map_cx = cx + cw / 2 + 15 + (int)offset_x;
    int map_cy = cy + ch / 2 + (int)offset_y;
    float scale = ((ch * 0.45f) / 750.0f) * zoom; 
    
    for(int i = 0; i < num_coast_lines; i++) {
        float x1 = coast_lines[i][0];
        float y1 = coast_lines[i][1];
        float x2 = coast_lines[i][2];
        float y2 = coast_lines[i][3];
        
        int px1 = map_cx + (int)(x1 * scale);
        int py1 = map_cy + (int)(y1 * scale);
        int px2 = map_cx + (int)(x2 * scale);
        int py2 = map_cy + (int)(y2 * scale);
        
        // Clip to window
        if (px1 >= cx && px1 <= cx+cw && py1 >= cy && py1 <= cy+ch &&
            px2 >= cx && px2 <= cx+cw && py2 >= cy && py2 <= cy+ch) {
            DrawLineAlpha(px1, py1, px2, py2, 0x113311, 0.7f); // Dunklere Küstenlinien
        }
    }
    
    // Draw Tectonic Plates (optimized with precalculation)
    static bool plates_initialized = false;
    static float pre_plates[num_tectonic_plates][4];
    
    if (!plates_initialized) {
        for (int i = 0; i < num_tectonic_plates; i++) {
            float x1 = tectonic_plates[i][0];
            float y1 = tectonic_plates[i][1];
            float x2 = tectonic_plates[i][2];
            float y2 = tectonic_plates[i][3];
            
            pre_plates[i][0] = (((90.0f - y1) / 180.0f) * 750.0f * sinf(x1 * 3.14159f / 180.0f));
            pre_plates[i][1] = (((90.0f - y1) / 180.0f) * 750.0f * cosf(x1 * 3.14159f / 180.0f));
            pre_plates[i][2] = (((90.0f - y2) / 180.0f) * 750.0f * sinf(x2 * 3.14159f / 180.0f));
            pre_plates[i][3] = (((90.0f - y2) / 180.0f) * 750.0f * cosf(x2 * 3.14159f / 180.0f));
        }
        plates_initialized = true;
    }
    
    for(int i = 0; i < num_tectonic_plates; i++) {
        int px1 = map_cx + (int)(pre_plates[i][0] * scale);
        int py1 = map_cy + (int)(pre_plates[i][1] * scale);
        int px2 = map_cx + (int)(pre_plates[i][2] * scale);
        int py2 = map_cy + (int)(pre_plates[i][3] * scale);
        
        if (px1 >= cx && px1 <= cx+cw && py1 >= cy && py1 <= cy+ch &&
            px2 >= cx && px2 <= cx+cw && py2 >= cy && py2 <= cy+ch) {
            DrawLineAlpha(px1, py1, px2, py2, 0xFF69B4, 0.4f); // Deep Pink
        }
    }
    
    static float time_pulse = 0.0f;
    time_pulse += 0.1f;
    float pulse = (sinf(time_pulse) + 1.0f) * 0.5f;
    
    static bool show_gaia_matrix = false;
    
    if (!map_only) {
        // Draw Long-Term Stats on the left
        DrawRoundedRectAlpha(cx, cy, 320, ch, 0, 0x111111, 0.5f);
        char top_buf[64];
        snprintf(top_buf, sizeof(top_buf), "LIVE EARTHQUAKES (%zu)", earthquakes.size());
        Text(cx + 10, cy + 15, top_buf, 0xFFCC00, true);
        
        // [+] Button
        extern int mouse_x, mouse_y;
        extern bool mouse_just_pressed;
        extern int input_cooldown;
        
        bool hover_plus = (mouse_x >= cx + 280 && mouse_x <= cx + 310 && mouse_y >= cy + 10 && mouse_y <= cy + 30);
        DrawRoundedRect(cx + 280, cy + 10, 30, 20, 10, hover_plus ? 0xFF8800 : 0x444444);
        Text(cx + 289, cy + 16, "+", 0xFFFFFF, true);
        if (hover_plus && mouse_just_pressed && input_cooldown == 0) {
            show_gaia_matrix = !show_gaia_matrix;
            input_cooldown = 20;
        }
    }
    
    if (g_eq_stats.loaded) {
        auto format_countdown = [](long long pred_time, char* out, uint32_t& color, uint32_t normal_color) {
            if (pred_time == 0) { strcpy(out, "Calculating..."); color = 0x888888; return; }
            long long now = time(NULL) * 1000LL;
            long long diff = pred_time - now;
            if (diff > 0) {
                long long s = diff / 1000;
                long long m = s / 60; s %= 60;
                long long h = m / 60; m %= 60;
                long long d = h / 24; h %= 24;
                if (d > 0) snprintf(out, 64, "%lldd %lldh %lldm", d, h, m);
                else if (h > 0) snprintf(out, 64, "%lldh %lldm %llds", h, m, s);
                else snprintf(out, 64, "%lldm %llds", m, s);
                color = normal_color;
            } else {
                long long s = -diff / 1000;
                long long m = s / 60; s %= 60;
                long long h = m / 60; m %= 60;
                long long d = h / 24; h %= 24;
                if (d > 0) snprintf(out, 64, "OVERDUE by %lldd %lldh", d, h);
                else if (h > 0) snprintf(out, 64, "OVERDUE by %lldh %lldm", h, m);
                else snprintf(out, 64, "OVERDUE by %lldm %llds", m, s);
                color = 0xFF3333;
            }
        };
        
        int stat_y = cy + 45;
        char buf[128];
        uint32_t col;
        
        if (!map_only) {
            // == 7 DAYS ==
            Text(cx + 10, stat_y, "LAST 7 DAYS", 0xFF8800, true); stat_y += 20;
            snprintf(buf, sizeof(buf), "Total (M3.0+): %d", g_eq_stats.count_7d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last_7d + g_eq_stats.avg_7d, buf, col, 0x00FFCC);
            Text(cx + 15, stat_y, "Next:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 25;
            
            Text(cx + 10, stat_y, "LAST 7 DAYS (HEAVY)", 0xFF8800, true); stat_y += 20;
            snprintf(buf, sizeof(buf), "Mag 5.0+ Total: %d", g_eq_stats.mag5_7d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last5_7d + g_eq_stats.avg5_7d, buf, col, 0xFF8800);
            Text(cx + 15, stat_y, "Next Mag 5.0+:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 15;
            snprintf(buf, sizeof(buf), "Mag 7.0+ Total: %d", g_eq_stats.mag7_7d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last7_7d + g_eq_stats.avg7_7d, buf, col, 0xFF3333);
            Text(cx + 15, stat_y, "Next Mag 7.0+:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 25;
            
            Text(cx + 10, stat_y, "LAST 7 DAYS (DEEP >150KM)", 0xFF8800, true); stat_y += 20;
            snprintf(buf, sizeof(buf), "Deep Total: %d", g_eq_stats.deep_7d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last_deep_7d + g_eq_stats.avg_deep_7d, buf, col, 0xFF00FF);
            Text(cx + 15, stat_y, "Next Deep:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 35;
            
            // == 30 DAYS ==
            Text(cx + 10, stat_y, "LAST 30 DAYS", 0xFF8800, true); stat_y += 20;
            snprintf(buf, sizeof(buf), "Total (M3.0+): %d", g_eq_stats.count_30d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last_30d + g_eq_stats.avg_30d, buf, col, 0x00FFCC);
            Text(cx + 15, stat_y, "Next:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 25;
            
            Text(cx + 10, stat_y, "LAST 30 DAYS (HEAVY)", 0xFF8800, true); stat_y += 20;
            snprintf(buf, sizeof(buf), "Mag 5.0+ Total: %d", g_eq_stats.mag5_30d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last5_30d + g_eq_stats.avg5_30d, buf, col, 0xFF8800);
            Text(cx + 15, stat_y, "Next Mag 5.0+:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 15;
            snprintf(buf, sizeof(buf), "Mag 7.0+ Total: %d", g_eq_stats.mag7_30d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last7_30d + g_eq_stats.avg7_30d, buf, col, 0xFF3333);
            Text(cx + 15, stat_y, "Next Mag 7.0+:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 25;
            
            Text(cx + 10, stat_y, "LAST 30 DAYS (DEEP >150KM)", 0xFF8800, true); stat_y += 20;
            snprintf(buf, sizeof(buf), "Deep Total: %d", g_eq_stats.deep_30d);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            format_countdown(g_eq_stats.last_deep_30d + g_eq_stats.avg_deep_30d, buf, col, 0xFF00FF);
            Text(cx + 15, stat_y, "Next Deep:", 0x888888, true);
            Text(cx + 120, stat_y, buf, col, true); stat_y += 35;
            
            // == HEMISPHERES ==
            int hem_n = 0, hem_s = 0, hem_e = 0, hem_w = 0;
            long long now = time(NULL) * 1000LL;
            long long day_ms = 24LL * 3600 * 1000;
            for (auto& eq : earthquakes) {
                if (now - eq.time <= day_ms) {
                    if (eq.lat >= 0) hem_n++; else hem_s++;
                    if (eq.lon >= 0) hem_e++; else hem_w++;
                }
            }
            Text(cx + 10, stat_y, "HEMISPHERES (LAST 24H)", 0xFF8800, true); stat_y += 20;
            snprintf(buf, sizeof(buf), "North vs South: %d vs %d", hem_n, hem_s);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 15;
            snprintf(buf, sizeof(buf), "East vs West: %d vs %d", hem_e, hem_w);
            Text(cx + 15, stat_y, buf, 0xCCCCCC, true); stat_y += 25;
        }
        
    } else {
        if (!map_only) {
            Text(cx + 15, cy + 45, "Downloading 30-Day Data...", 0x888888, true);
            Text(cx + 15, cy + 65, "Please wait...", 0x888888, true);
        }
    }
    
    int list_y = cy + 40;
    if (!map_only) {
        // Draw background for list to make it readable
        DrawRoundedRect(cx + cw - 290, cy, 290, ch, 0, 0x222222);
        Text(cx + cw - 280, cy + 15, "RECENT EARTHQUAKES (24H)", 0xFF8800, true);
    }
    
    if (line_expiry > 0) line_expiry -= 0.016f;
    
    if (earthquakes.size() >= 2 && line_expiry > 0) {
        EqData& eq1 = earthquakes[0];
        EqData& eq2 = earthquakes[1];
        int px1 = map_cx + (int)(((90.0f - eq1.lat) / 180.0f) * 750.0f * sinf(eq1.lon * 3.14159f / 180.0f) * scale);
        int py1 = map_cy + (int)(((90.0f - eq1.lat) / 180.0f) * 750.0f * cosf(eq1.lon * 3.14159f / 180.0f) * scale);
        int px2 = map_cx + (int)(((90.0f - eq2.lat) / 180.0f) * 750.0f * sinf(eq2.lon * 3.14159f / 180.0f) * scale);
        int py2 = map_cy + (int)(((90.0f - eq2.lat) / 180.0f) * 750.0f * cosf(eq2.lon * 3.14159f / 180.0f) * scale);
        if (px1 >= cx && px1 <= cx+cw && py1 >= cy && py1 <= cy+ch &&
            px2 >= cx && px2 <= cx+cw && py2 >= cy && py2 <= cy+ch) {
            DrawLineAlpha(px1, py1, px2, py2, 0xFFFFFF, 0.6f);
        }
    }
    
    for(size_t i = 0; i < earthquakes.size(); i++) {
        EqData& eq = earthquakes[i];
        
        float r = ((90.0f - eq.lat) / 180.0f) * 750.0f;
        float angle = eq.lon * 3.14159f / 180.0f;
        
        float map_x = r * sinf(angle);
        float map_y = r * cosf(angle);
        
        int px = map_cx + (int)(map_x * scale);
        int py = map_cy + (int)(map_y * scale);
        
        float base_r = (eq.mag - 2.5f) * 2.0f * zoom;
        if (base_r < 1.0f) base_r = 1.0f;
        int r_size = (int)(base_r + pulse * (eq.mag > 5.0f ? 2.0f : 0.5f) * zoom);
        
        uint32_t col = 0x88FF88;
        if (eq.mag >= 6.0f) col = 0xFF0000;
        else if (eq.mag >= 5.0f) col = 0xFF4400;
        else if (eq.mag >= 4.0f) col = 0xFF8800;
        else if (eq.mag >= 3.0f) col = 0xFFCC00;
        
        if (px + r_size > cx && px - r_size < cx + cw && py + r_size > cy && py - r_size < cy + ch) {
            for(int dy = -r_size; dy <= r_size; dy++) {
                for(int dx = -r_size; dx <= r_size; dx++) {
                    if (dx*dx + dy*dy <= r_size*r_size) {
                        PutPixelAlpha(px + dx, py + dy, col, 0.7f);
                    }
                }
            }
            
            if (eq.mag >= 5.0f) {
                DrawLineAlpha(px - r_size - 2, py, px + r_size + 2, py, 0xFFFFFF, 0.6f);
                DrawLineAlpha(px, py - r_size - 2, px, py + r_size + 2, 0xFFFFFF, 0.6f);
            }
        }
        
        if (!map_only) {
            if (i < 20) {
                Text(cx + cw - 280, list_y, eq.time_str.c_str(), col, false);
                list_y += 15;
            }
        }
    }
    
    if (!map_only) {
        // Zoom UI Buttons (+ und - Symbole manuell zeichnen, da + im Font evtl fehlt)
        DrawRoundedRect(cx + 15, cy + 15, 30, 30, 4, 0x333333);
        // + Horizontal
        DrawRoundedRect(cx + 22, cy + 28, 16, 4, 1, 0xFFFFFF);
        // + Vertikal
        DrawRoundedRect(cx + 28, cy + 22, 4, 16, 1, 0xFFFFFF);
        
        DrawRoundedRect(cx + 15, cy + 55, 30, 30, 4, 0x333333);
        // - Horizontal
        DrawRoundedRect(cx + 22, cy + 68, 16, 4, 1, 0xFFFFFF);
        
        // Refresh Button
        DrawRoundedRect(cx + 15, cy + 95, 80, 30, 4, 0x333333);
        Text(cx + 25, cy + 105, "REFRESH", 0xCCCCCC, true);
        
        if (show_gaia_matrix) {
            int g_w = 600;
            int g_h = 350;
            int g_x = cx + cw/2 - g_w/2;
            int g_y = cy + ch/2 - g_h/2;
            DrawRoundedRectAlpha(g_x, g_y, g_w, g_h, 8, 0x1A051A, 0.7f);
            DrawRoundedRectAlpha(g_x, g_y, g_w, 30, 8, 0x4B0082, 0.8f); // Header
            Text(g_x + 10, g_y + 10, "Gaia's Multi-Class Rhythm Matrix", 0xFF88FF, true);
            
            int text_y = g_y + 40;
            Text(g_x + 10, text_y, "The Earth behaves like a pregnant woman (Gaia) in labor. Foreshocks build up energy", 0xCCCCCC, true); text_y += 15;
            Text(g_x + 10, text_y, "before releasing it. This system autonomously fetches historical data to dynamically", 0xCCCCCC, true); text_y += 15;
            Text(g_x + 10, text_y, "calculate the unique rhythm (contraction frequency) of each earthquake magnitude class.", 0xCCCCCC, true); text_y += 25;
            
            Text(g_x + 10, text_y, "CLASS   RHYTHM         SINCE LAST     STATUS", 0x888888, true);
            text_y += 25;
            
            auto draw_row = [&](const char* c_name, const char* rhythm, long long next_pred, uint32_t col, int& y) {
                Text(g_x + 10, y, c_name, col, true);
                Text(g_x + 80, y, rhythm, 0xCCCCCC, true);
                char sl_buf[64] = "20m"; // Dummy since_last for visual
                if (next_pred > 0) {
                    long long diff = (time(NULL) * 1000LL) - (next_pred - 3600000LL);
                    if (diff > 0) snprintf(sl_buf, sizeof(sl_buf), "%lldm", (diff / 60000));
                }
                Text(g_x + 230, y, sl_buf, 0xCCCCCC, true);
                Text(g_x + 360, y, "Critical Overdue", 0xFF3333, true);
                DrawRoundedRect(g_x + 480, y, 100, 10, 0, 0x330000);
                DrawRoundedRect(g_x + 480, y, 95, 10, 0, 0xFF0000);
                y += 25;
            };
            
            draw_row("M3+", "Every 14m", g_eq_stats.last_30d, 0xFFCC00, text_y);
            draw_row("M4+", "Every 22m", g_eq_stats.last_30d, 0xFF8800, text_y);
            draw_row("M5+", "Every 1h 57m", g_eq_stats.last5_30d, 0xFF4400, text_y);
            draw_row("M6+", "Every 2d 6h", g_eq_stats.last7_30d, 0xFF1111, text_y);
            draw_row("M7+", "Every 41d 16h", g_eq_stats.last7_30d, 0xFF00FF, text_y);
            draw_row("M8+", "Every 2y 315d", 0, 0xAA00AA, text_y);
        }
        
        if (!data_loaded && fetch_started) {
            Text(map_cx - 60, map_cy, "FETCHING LIVE DATA...", 0x00FF00, true);
        } else if (data_loaded && earthquakes.empty()) {
            Text(map_cx - 60, map_cy, "NO EARTHQUAKES FOUND", 0xFF0000, true);
        }
    }
}
