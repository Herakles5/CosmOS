#ifndef COSMOS_ZT_H
#define COSMOS_ZT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char zt_target_id[17] = {0};
static int zt_target_id_len = 0;
static bool zt_input_focus = false;

static char zt_status_line[256] = "Offline";
static bool zt_has_network = false;

inline void zt_poll_status() {
    if (zt_target_id_len != 16) {
        zt_has_network = false;
        strcpy(zt_status_line, "Offline");
        return;
    }
    
    FILE* fp = popen("sudo zerotier-cli listnetworks 2>/dev/null", "r");
    if (!fp) return;
    
    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, zt_target_id)) {
            found = true;
            if (strlen(line) > 34) {
                strncpy(zt_status_line, line + 34, 250);
            } else {
                strncpy(zt_status_line, "Connecting...", 250);
            }
            for(int i=0; i<250; i++) { if(zt_status_line[i] == '\n') zt_status_line[i] = 0; }
            
            // Extract the IP and compute the broadcast IP for UDP chat
            char* last_space = strrchr(zt_status_line, ' ');
            if (last_space) {
                char ip_buf[64];
                strncpy(ip_buf, last_space + 1, 63); ip_buf[63] = 0;
                char* slash = strchr(ip_buf, '/');
                if (slash) *slash = 0; // Strip subnet mask
                char* last_dot = strrchr(ip_buf, '.');
                if (last_dot) {
                    strcpy(last_dot + 1, "255");
                    extern char chat_bcast_ip[32];
                    strcpy(chat_bcast_ip, ip_buf);
                }
            }
            break; 
        }
    }
    pclose(fp);
    
    zt_has_network = found;
    if (!found) {
        strcpy(zt_status_line, "Network not found or left");
    }
}

inline void zt_join() {
    if (zt_target_id_len == 16) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "sudo zerotier-cli join %s > /dev/null 2>&1", zt_target_id);
        system(cmd);
    }
}

inline void zt_leave() {
    if (zt_target_id_len == 16) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "sudo zerotier-cli leave %s > /dev/null 2>&1", zt_target_id);
        system(cmd);
    }
}

#endif
