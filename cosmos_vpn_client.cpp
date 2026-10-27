#include "cosmos_vpn_client.h"
#include "cosmos_vpn_crypto.h"
#include "cosmos_vpn_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>
#include <netinet/ip.h>

#define VPN_PORT 4242
// MTU 1400: Optimal compromise - higher payload than 1280, safely below
// 1500 limit for cellular networks (1400 + 8 UDP + 20 IP + 26 CosmOS = 1454 on wire)
// MTU 1500 causes fragmentation on Android/LTE due to GTP tunnel overhead!
#define MTU 1400
#define HEADER_SIZE 26

// Maximum UDP payload: Header + compressed batch
// ArShiva worst case: 9/8 * input + 2
#define MAX_COMPRESSED VPN_COMPRESS_BOUND(VPN_BATCH_MAX)
#define MAX_UDP_PAYLOAD (HEADER_SIZE + MAX_COMPRESSED)

static int tun_fd = -1;
static int udp_fd = -1;
static volatile int vpn_active = 0;
static struct sockaddr_in server_addr;
static pthread_t vpn_thread;
static char current_tun_name[IFNAMSIZ] = "";
static char vpn_server_ip[64] = "";

static uint8_t session_key[8];
static int session_established = 0;

// Extern function from arcade.cpp
extern void hs_add_log(const char* txt);

static int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;
    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
        return fd;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; 
    if( *dev ) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }
    if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
        close(fd);
        return err;
    }
    strcpy(dev, ifr.ifr_name);
    return fd;
}

// Callback: Writes an extracted sub-packet into TUN
static void write_tun_callback(const uint8_t* pkt, int pkt_len, void* ctx) {
    int fd = *(int*)ctx;
    write(fd, pkt, pkt_len);
}

static void* vpn_thread_func(void* arg) {
    unsigned char buf[MTU];
    unsigned char header[HEADER_SIZE];
    unsigned char comp_buf[MAX_COMPRESSED];   // Compressed output
    unsigned char decomp_buf[VPN_BATCH_MAX];  // Decompressed input
    unsigned char recv_buf[MAX_UDP_PAYLOAD];  // Receive buffer
    uint32_t frame_id = 0;
    
    // Batch buffer for outbound packets
    VpnBatchBuffer batch;
    batch.reset();
    
    // --- Handshake Phase (with retry logic) ---
    uint32_t c_nonce = (uint32_t)rand();
    cosmos_hs_packet hs_req;
    memset(&hs_req, 0, sizeof(hs_req));
    hs_req.magic_start[0] = 0x2A; hs_req.magic_start[1] = 0xC0;
    hs_req.type_flag = FLAG_HANDSHAKE_INIT;
    hs_req.client_nonce = c_nonce;
    hs_req.magic_end[0] = 0xEE; hs_req.magic_end[1] = 0xFF;
    
    // 3 attempts with increasing timeout (2s, 3s, 4s)
    for (int attempt = 0; attempt < 3 && !session_established; attempt++) {
        sendto(udp_fd, &hs_req, sizeof(hs_req), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        char log_buf[128];
        snprintf(log_buf, sizeof(log_buf), "VPN: Handshake INIT attempt %d/3...", attempt + 1);
        hs_add_log(log_buf);
        
        struct timeval tv_hs;
        tv_hs.tv_sec = 2 + attempt; // Increasing timeout: 2s, 3s, 4s
        tv_hs.tv_usec = 0;
        fd_set hs_fds;
        FD_ZERO(&hs_fds);
        FD_SET(udp_fd, &hs_fds);
        
        if (select(udp_fd + 1, &hs_fds, NULL, NULL, &tv_hs) > 0) {
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);
            int nread = recvfrom(udp_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&sender_addr, &sender_len);
            if (nread >= (int)sizeof(cosmos_hs_packet)) {
                cosmos_hs_packet* reply = (cosmos_hs_packet*)recv_buf;
                if (reply->magic_start[0] == 0x2A && reply->magic_start[1] == 0xC0 && 
                    reply->magic_end[0] == 0xEE && reply->magic_end[1] == 0xFF &&
                    reply->type_flag == FLAG_HANDSHAKE_REPLY) {
                    
                    compute_session_key(c_nonce, reply->server_nonce, session_key);
                    session_established = 1;
                    hs_add_log("VPN: SECURE Session Key Established!");
                }
            }
        }
    }
    
    if (!session_established) {
        hs_add_log("VPN ERROR: Handshake failed after 3 attempts!");
        vpn_active = 0;
        return NULL;
    }
    
    // --- Set routes ONLY AFTER successful handshake ---
    // Bypass route for VPN server itself (so UDP packets continue directly)
    {
        char route_cmd[256];
        snprintf(route_cmd, sizeof(route_cmd),
            "DEF=$(ip route | grep default | head -n 1 | sed 's/default //'); ip route add %s $DEF 2>/dev/null",
            vpn_server_ip);
        system(route_cmd);
    }
    // Split tunneling: route only internal VPN network through tunnel!
    // Leaves all other internet traffic untouched and fast.
    system("ip route add 10.0.0.0/8 via 10.42.0.1 2>/dev/null");
    hs_add_log("VPN: Split-Tunneling activated – only 10.x.x.x through tunnel.");
    // --- End of Handshake ---
    
    // poll() instead of select() - better for timeout-based batching
    struct pollfd pfds[2];
    pfds[0].fd = tun_fd;
    pfds[0].events = POLLIN;
    pfds[1].fd = udp_fd;
    pfds[1].events = POLLIN;
    
    while (vpn_active) {
        // Timeout: if batch is not empty, short timeout for flushing
        int timeout_ms = batch.is_empty() ? 1000 : VPN_BATCH_TIMEOUT_MS;
        
        int activity = poll(pfds, 2, timeout_ms);
        
        // Batch flush: either timeout expired or batch full
        auto flush_batch = [&]() {
            if (batch.is_empty()) return;
            
            // 1. Compress
            int comp_len = vpn_compress(batch.data, batch.used, comp_buf, sizeof(comp_buf));
            if (comp_len <= 0) {
                // Compression failed - send uncompressed
                memcpy(comp_buf, batch.data, batch.used);
                comp_len = batch.used;
            }
            
            // 2. Calculate checksum on PLAINTEXT
            uint16_t checksum = calculate_checksum(comp_buf, comp_len);
            
            // 3. Encrypt (after checksum and compression!)
            encrypt_decrypt_payload(comp_buf, comp_len, session_key, frame_id);
            
            // 4. Build header
            memset(header, 0, HEADER_SIZE);
            header[0] = 0x2A; header[1] = 0xC0;
            // [2..3]: Compressed payload size
            header[2] = (comp_len >> 8) & 0xFF;
            header[3] = comp_len & 0xFF;
            // [4..5]: Original (uncompressed) size
            header[4] = (batch.used >> 8) & 0xFF;
            header[5] = batch.used & 0xFF;
            // [15..17]: "VPN"
            header[15] = 'V'; header[16] = 'P'; header[17] = 'N';
            // [18..21]: Frame ID
            header[18] = (frame_id >> 24) & 0xFF;
            header[19] = (frame_id >> 16) & 0xFF;
            header[20] = (frame_id >> 8) & 0xFF;
            header[21] = frame_id & 0xFF;
            // [22..23]: Checksum (calculated over PLAINTEXT)
            header[22] = (checksum >> 8) & 0xFF;
            header[23] = checksum & 0xFF;
            header[24] = 0xEE; header[25] = 0xFF;
            
            // 4. Scatter/Gather I/O: sendmsg() instead of memcpy+sendto()
            struct iovec iov[2];
            iov[0].iov_base = header;
            iov[0].iov_len  = HEADER_SIZE;
            iov[1].iov_base = comp_buf;
            iov[1].iov_len  = comp_len;
            
            struct msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_name    = &server_addr;
            msg.msg_namelen = sizeof(server_addr);
            msg.msg_iov     = iov;
            msg.msg_iovlen  = 2;
            
            sendmsg(udp_fd, &msg, 0);
            frame_id++;
            batch.reset();
        };
        
        if (activity > 0) {
            // Read from TUN -> collect batch -> send if necessary
            if (pfds[0].revents & POLLIN) {
                while (1) {
                    int nread = read(tun_fd, buf, sizeof(buf));
                    if (nread <= 0) break;
                    if (!batch.add_packet(buf, nread)) {
                        // Batch full - flush first, then add new packet
                        flush_batch();
                        batch.add_packet(buf, nread);
                    }
                    // If batch is full enough, send immediately
                    if (batch.is_full()) {
                        flush_batch();
                    }
                }
            }
            
            // Read from UDP (Server) -> Decrypt -> Decompress -> TUN
            if (pfds[1].revents & POLLIN) {
                struct sockaddr_in sender_addr;
                socklen_t sender_len = sizeof(sender_addr);
                int nread = recvfrom(udp_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&sender_addr, &sender_len);
                if (nread >= HEADER_SIZE) {
                    if (recv_buf[0] == 0x2A && recv_buf[1] == 0xC0 && recv_buf[24] == 0xEE && recv_buf[25] == 0xFF) {
                        uint16_t payload_size = (recv_buf[2] << 8) | recv_buf[3];
                        uint16_t original_size = (recv_buf[4] << 8) | recv_buf[5];
                        
                        if (payload_size > 0 && nread == HEADER_SIZE + payload_size) {
                            uint32_t rx_frame_id = (recv_buf[18] << 24) | (recv_buf[19] << 16) | (recv_buf[20] << 8) | recv_buf[21];
                            
                            // 1. Decrypt FIRST (since server calculated checksum on plaintext)
                            encrypt_decrypt_payload(recv_buf + HEADER_SIZE, payload_size, session_key, rx_frame_id);
                            
                            uint16_t expected_checksum = (recv_buf[22] << 8) | recv_buf[23];
                            uint16_t actual_checksum = calculate_checksum(recv_buf + HEADER_SIZE, payload_size);
                            
                            if (expected_checksum == actual_checksum) {
                                // 2. Decompress
                                if (original_size > 0 && original_size != payload_size) {
                                    int decomp_len = vpn_decompress(recv_buf + HEADER_SIZE, payload_size, decomp_buf, sizeof(decomp_buf));
                                    if (decomp_len > 0) {
                                        // 3. Extract sub-packets from batch and write to TUN
                                        vpn_batch_extract(decomp_buf, decomp_len, write_tun_callback, &tun_fd);
                                    }
                                } else {
                                    // Uncompressed (fallback) - extract sub-packets directly
                                    vpn_batch_extract(recv_buf + HEADER_SIZE, payload_size, write_tun_callback, &tun_fd);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // Timeout - flush batch if not empty
            flush_batch();
        }
    }
    return NULL;
}

void init_cosmos_vpn() {
    // Initialization if needed
}

void activate_cosmos_vpn(const char* server_ip) {
    if (vpn_active) return;
    
    current_tun_name[0] = '\0'; // Let kernel pick the name
    tun_fd = tun_alloc(current_tun_name);
    if (tun_fd < 0) {
        hs_add_log("VPN ERROR: FAILED TO CREATE TUN INTERFACE!");
        return;
    }
    
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        perror("socket");
        exit(1);
    }
    
    // === KERNEL TUNING (automatically upon VPN activation) ===
    system("sysctl -w net.core.rmem_max=16777216 > /dev/null 2>&1");
    system("sysctl -w net.core.wmem_max=16777216 > /dev/null 2>&1");
    system("sysctl -w net.core.rmem_default=1048576 > /dev/null 2>&1");
    system("sysctl -w net.core.wmem_default=1048576 > /dev/null 2>&1");
    system("sysctl -w net.ipv4.tcp_congestion_control=bbr > /dev/null 2>&1");
    system("sysctl -w net.core.default_qdisc=fq > /dev/null 2>&1");
    system("sysctl -w net.ipv4.tcp_fastopen=3 > /dev/null 2>&1");
    system("sysctl -w net.core.netdev_max_backlog=5000 > /dev/null 2>&1");
    
    // Increase socket buffers (4 MB - now permitted by kernel)
    int sock_buf_size = 4194304;
    setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, &sock_buf_size, sizeof(sock_buf_size));
    setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, &sock_buf_size, sizeof(sock_buf_size));
    
    // DSCP: Mark VPN packets as Expedited Forwarding (highest priority)
    int tos = 0xB8; // DSCP 46 = EF (Expedited Forwarding)
    setsockopt(udp_fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
    
    // Set TUN to non-blocking for drain loop
    fcntl(tun_fd, F_SETFL, O_NONBLOCK);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(VPN_PORT);
    
    // System commands to configure interface (runs as root)
    char cmd[256];
    sprintf(cmd, "ip link set dev %s up", current_tun_name);
    system(cmd);
    // MTU 1400: More payload than 1280, safely below 1500 for cellular/CGNAT
    sprintf(cmd, "ip link set dev %s mtu %d", current_tun_name, MTU);
    system(cmd);
    sprintf(cmd, "ip addr add 10.42.0.2/24 dev %s", current_tun_name);
    system(cmd);
    
    // fq_codel on TUN interface: Bufferbloat protection in tunnel
    sprintf(cmd, "tc qdisc replace dev %s root fq_codel 2>/dev/null", current_tun_name);
    system(cmd);
    
    // MSS Clamping: automatically adapt TCP segment size to VPN MTU
    sprintf(cmd, "iptables -A FORWARD -o %s -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu", current_tun_name);
    system(cmd);

    // Routes are now set in vpn_thread AFTER successful handshake
    // (previously: routes set BEFORE handshake → Blackhole!)
    
    vpn_active = 1;
    session_established = 0;
    strncpy(vpn_server_ip, server_ip, sizeof(vpn_server_ip) - 1);
    vpn_server_ip[sizeof(vpn_server_ip) - 1] = '\0';
    pthread_create(&vpn_thread, NULL, vpn_thread_func, NULL);
    
    hs_add_log("SYSTEM: 26-BYTE VPN ACTIVATED! (MTU=1400, ArShiva, Batch, 64-Bit Crypto, BBR, 4MB Buffer)");
}

void deactivate_cosmos_vpn() {
    if (!vpn_active) return;
    vpn_active = 0;
    
    // DEADLOCK FIX: Close socket BEFORE waiting for thread.
    // This causes poll() in thread to immediately terminate with error.
    if (udp_fd >= 0) close(udp_fd);
    if (tun_fd >= 0) close(tun_fd);
    
    pthread_join(vpn_thread, NULL);
    
    tun_fd = -1;
    udp_fd = -1;
    char cmd[256];
    sprintf(cmd, "ip link set dev %s down", current_tun_name);
    system(cmd);
    
    // Remove MSS clamping rule
    sprintf(cmd, "iptables -D FORWARD -o %s -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu 2>/dev/null", current_tun_name);
    system(cmd);
    
    // Remove split tunneling route
    system("ip route del 10.0.0.0/8 via 10.42.0.1 2>/dev/null");
    
    {
        char route_cmd[256];
        snprintf(route_cmd, sizeof(route_cmd),
            "DEF=$(ip route | grep default | head -n 1 | sed 's/default //'); ip route del %s $DEF 2>/dev/null",
            vpn_server_ip);
        system(route_cmd);
    }

    hs_add_log("SYSTEM: 26-BYTE VPN DEACTIVATED!");
}

int is_cosmos_vpn_active() {
    return vpn_active;
}
