#ifndef COSMOS_CHAT_H
#define COSMOS_CHAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <time.h>

#define CHAT_PORT 13337
#define MAX_CHAT_LINES 15
#define MAX_PEERS 32

struct ChatPeer {
    char name[32];
    char ip[32];
    time_t last_seen;
    bool blocked;
};

static int chat_sock = -1;
static ChatPeer chat_peers[MAX_PEERS];
static int num_chat_peers = 0;

static char chat_history[MAX_CHAT_LINES][128];
static int chat_history_count = 0;
extern char os2_user_name[32];
static char chat_input[128] = {0};
static int chat_input_len = 0;
static bool chat_input_focus = false;
static char current_group[32] = "";


inline void chat_add_line(const char* line) {
    if (chat_history_count < MAX_CHAT_LINES) {
        strncpy(chat_history[chat_history_count++], line, 127);
    } else {
        for (int i = 1; i < MAX_CHAT_LINES; i++) {
            strcpy(chat_history[i-1], chat_history[i]);
        }
        strncpy(chat_history[MAX_CHAT_LINES-1], line, 127);
    }
}

inline void chat_init() {
    if (chat_sock != -1) return;
    chat_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (chat_sock < 0) return;
    
    int opt = 1;
    setsockopt(chat_sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(chat_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CHAT_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    bind(chat_sock, (struct sockaddr*)&addr, sizeof(addr));
    
    int flags = fcntl(chat_sock, F_GETFL, 0);
    fcntl(chat_sock, F_SETFL, flags | O_NONBLOCK);
    
    chat_add_line("[SYSTEM] UDP LAN Lobby initialized.");
}

char chat_bcast_ip[32] = "255.255.255.255";

inline void chat_broadcast(const char* msg) {
    if (chat_sock < 0) return;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CHAT_PORT);
    addr.sin_addr.s_addr = inet_addr(chat_bcast_ip);
    
    sendto(chat_sock, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr));
}

inline void chat_send_msg(const char* text) {
    if (strncmp(text, "/pm ", 4) == 0) {
        char target[32] = {0};
        char msg[128] = {0};
        const char* space = strchr(text + 4, ' ');
        if (space) {
            int len = space - (text + 4);
            if (len > 31) len = 31;
            strncpy(target, text + 4, len);
            strncpy(msg, space + 1, 127);
            
            char packet[256];
            snprintf(packet, sizeof(packet), "PM:%s:%s:%s", target, os2_user_name, msg);
            chat_broadcast(packet);
            
            char local_disp[256];
            snprintf(local_disp, sizeof(local_disp), "[PM to %s]: %s", target, msg);
            chat_add_line(local_disp);
        } else {
            chat_add_line("[SYSTEM] Usage: /pm <name> <msg>");
        }
        return;
    }
    
    if (strncmp(text, "/join ", 6) == 0) {
        strncpy(current_group, text + 6, 31);
        char buf[128];
        snprintf(buf, sizeof(buf), "[SYSTEM] Joined group: %s", current_group);
        chat_add_line(buf);
        return;
    }
    
    if (strcmp(text, "/leave") == 0) {
        current_group[0] = 0;
        chat_add_line("[SYSTEM] Left group. You are in the Lobby.");
        return;
    }

    char packet[256];
    char local_disp[256];
    if (current_group[0] != 0) {
        snprintf(packet, sizeof(packet), "GRP:%s:%s: %s", current_group, os2_user_name, text);
        snprintf(local_disp, sizeof(local_disp), "[%s] %s: %s", current_group, os2_user_name, text);
    } else {
        snprintf(packet, sizeof(packet), "MSG:%s: %s", os2_user_name, text);
        snprintf(local_disp, sizeof(local_disp), "%s: %s", os2_user_name, text);
    }
    chat_broadcast(packet);
    chat_add_line(local_disp);
}

inline void chat_send_ping() {
    char packet[128];
    snprintf(packet, sizeof(packet), "PING:%s", os2_user_name);
    chat_broadcast(packet);
}

inline void chat_poll() {
    if (chat_sock < 0) return;
    char buffer[256];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    
    while (true) {
        int n = recvfrom(chat_sock, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&from, &fromlen);
        if (n <= 0) break;
        buffer[n] = 0;
        
        char from_ip[32];
        strcpy(from_ip, inet_ntoa(from.sin_addr));
        
        if (strncmp(buffer, "PING:", 5) == 0) {
            char* name = buffer + 5;
            if (strcmp(name, os2_user_name) == 0) continue; 
            
            bool found = false;
            for (int i = 0; i < num_chat_peers; i++) {
                if (strcmp(chat_peers[i].ip, from_ip) == 0) {
                    strcpy(chat_peers[i].name, name);
                    chat_peers[i].last_seen = time(NULL);
                    found = true;
                    break;
                }
            }
            if (!found && num_chat_peers < MAX_PEERS) {
                strcpy(chat_peers[num_chat_peers].ip, from_ip);
                strcpy(chat_peers[num_chat_peers].name, name);
                chat_peers[num_chat_peers].last_seen = time(NULL);
                chat_peers[num_chat_peers].blocked = false;
                num_chat_peers++;
                
                char join_msg[128];
                snprintf(join_msg, sizeof(join_msg), "[SYSTEM] %s joined the lobby.", name);
                chat_add_line(join_msg);
            }
        }
        else if (strncmp(buffer, "MSG:", 4) == 0) {
            if (current_group[0] != 0) continue; // Ignore lobby msgs in group
            char* colon = strchr(buffer + 4, ':');
            if (colon) {
                char name[32] = {0};
                int nlen = colon - (buffer + 4);
                if (nlen > 31) nlen = 31;
                strncpy(name, buffer + 4, nlen);
                
                if (strcmp(name, os2_user_name) == 0) continue; 
                
                bool blocked = false;
                for (int i = 0; i < num_chat_peers; i++) {
                    if (strcmp(chat_peers[i].ip, from_ip) == 0 && chat_peers[i].blocked) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) {
                    chat_add_line(buffer + 4); 
                }
            }
        }
        else if (strncmp(buffer, "PM:", 3) == 0) {
            char* c1 = strchr(buffer + 3, ':');
            if (c1) {
                char* c2 = strchr(c1 + 1, ':');
                if (c2) {
                    char target[32] = {0};
                    strncpy(target, buffer + 3, c1 - (buffer + 3));
                    if (strcmp(target, os2_user_name) == 0) {
                        char sender[32] = {0};
                        strncpy(sender, c1 + 1, c2 - (c1 + 1));
                        
                        bool blocked = false;
                        for (int i = 0; i < num_chat_peers; i++) {
                            if (strcmp(chat_peers[i].ip, from_ip) == 0 && chat_peers[i].blocked) {
                                blocked = true; break;
                            }
                        }
                        if (!blocked) {
                            char disp[256];
                            snprintf(disp, sizeof(disp), "[PM from %s]:%s", sender, c2 + 1);
                            chat_add_line(disp);
                        }
                    }
                }
            }
        }
        else if (strncmp(buffer, "GRP:", 4) == 0) {
            char* c1 = strchr(buffer + 4, ':');
            if (c1) {
                char group[32] = {0};
                strncpy(group, buffer + 4, c1 - (buffer + 4));
                if (current_group[0] != 0 && strcmp(group, current_group) == 0) {
                    char* c2 = strchr(c1 + 1, ':');
                    if (c2) {
                        char sender[32] = {0};
                        strncpy(sender, c1 + 1, c2 - (c1 + 1));
                        if (strcmp(sender, os2_user_name) == 0) continue;
                        
                        bool blocked = false;
                        for (int i = 0; i < num_chat_peers; i++) {
                            if (strcmp(chat_peers[i].ip, from_ip) == 0 && chat_peers[i].blocked) {
                                blocked = true; break;
                            }
                        }
                        if (!blocked) {
                            char disp[256];
                            snprintf(disp, sizeof(disp), "[%s] %s:%s", group, sender, c2 + 1);
                            chat_add_line(disp);
                        }
                    }
                }
            }
        }
    }
    
    time_t now = time(NULL);
    for (int i = 0; i < num_chat_peers; i++) {
        if (now - chat_peers[i].last_seen > 10) {
            char left_msg[128];
            snprintf(left_msg, sizeof(left_msg), "[SYSTEM] %s left.", chat_peers[i].name);
            chat_add_line(left_msg);
            
            for (int j = i; j < num_chat_peers - 1; j++) {
                chat_peers[j] = chat_peers[j+1];
            }
            num_chat_peers--;
            i--;
        }
    }
}
#endif
