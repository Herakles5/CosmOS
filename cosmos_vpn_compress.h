#pragma once
// ==========================================
// COSMOS VPN COMPRESSION – ArShiva LZ77 Hybrid
// Extracted from ArShiva_fixed.cpp (BastionEngine)
// Compresses BEFORE encryption, decompresses AFTER decryption
// ==========================================
#include <string.h>
#include <stdint.h>

// Maximum batch size (for batching: multiple TUN packets collected)
// 16KB = ~10 TUN packets at MTU 1400 → less encryption/header overhead
#define VPN_BATCH_MAX          16384
// Maximum wait time to collect packets (milliseconds)
// Aggressive flushing: high cellular latency makes prolonged collection useless
#define VPN_BATCH_TIMEOUT_MS   2
// Maximum number of sub-packets per batch
#define VPN_BATCH_MAX_PACKETS  64

// ArShiva VPN effort: Minimal compression for maximum throughput
// HTTPS traffic is incompressible – save CPU rather than pointless compression!
#define VPN_COMPRESS_EFFORT    1
// Maximum match chain per effort
#define VPN_MAX_CHAIN          (VPN_COMPRESS_EFFORT * 1000)
#define VPN_MAX_CHAIN_TOTAL    (VPN_COMPRESS_EFFORT * 500)

// Worst-Case: Incompressible → 1 bit flag + 8 bits per byte = 9/8 ratio + padding
#define VPN_COMPRESS_BOUND(size) ((size) + ((size) / 8) + 2)

// ==========================================
// ArShiva Bit-Level I/O (from BastionEngine)
// ==========================================

static inline void arshiva_write_bits(uint8_t* buf, int* buf_len, 
                                       uint32_t val, int count, 
                                       uint32_t* acc, int* nB) {
    *acc |= (val << *nB);
    *nB += count;
    while (*nB >= 8) {
        buf[(*buf_len)++] = (uint8_t)(*acc & 0xFF);
        *acc >>= 8;
        *nB -= 8;
    }
}

static inline uint32_t arshiva_read_bits(uint32_t* bitPtr, const uint8_t* data, 
                                          int data_len, int count) {
    uint32_t val = 0;
    for (int i = 0; i < count; i++) {
        uint32_t byteIdx = (*bitPtr + i) / 8;
        if ((int)byteIdx >= data_len) return 0;
        if ((data[byteIdx] >> ((*bitPtr + i) % 8)) & 1)
            val |= (1U << i);
    }
    *bitPtr += count;
    return val;
}

// ==========================================
// ArShiva LZ77 Compression (VPN optimized)
// Tuned for speed in real-time operation
// ==========================================

// Compresses src → dst. Returns compressed size, or 0 on error / no gain.
static inline int vpn_compress(const uint8_t* src, int src_len, 
                                uint8_t* dst, int dst_capacity) {
    if (src_len <= 0 || dst_capacity <= 0) return 0;
    if (src_len < 4) {
        // Too short to compress – copy directly
        if (dst_capacity < src_len) return 0;
        memcpy(dst, src, src_len);
        return src_len;
    }
    
    uint32_t acc = 0;
    int nB = 0;
    int dst_len = 0;
    
    // Hash chain table (16-bit hash → last position)
    // Stack allocated for VPN packet sizes (max ~4KB)
    int head[65536];
    memset(head, -1, sizeof(head));
    
    // Prev chain (for each position, the previous one with same hash)
    // Max VPN_BATCH_MAX entries
    int prev[VPN_BATCH_MAX];
    memset(prev, -1, src_len < (int)sizeof(prev) ? src_len * sizeof(int) : sizeof(prev));
    
    // Build hash chain
    for (int k = 0; k + 2 < src_len; k++) {
        uint32_t h = ((uint32_t)src[k] << 8 | src[k+1]) ^ ((uint32_t)src[k+2] << 4);
        prev[k] = head[h & 0xFFFF];
        head[h & 0xFFFF] = k;
    }
    
    int i = 0;
    int chainsUsed = 0;
    
    while (i < src_len) {
        // Safety check: is output already larger than input?
        if (dst_len >= src_len - 4) return 0; // No gain
        
        int best_len = 0, best_dist = 0;
        
        if (i + 4 < src_len) {
            int cur = prev[i];
            int chain = 0;
            while (cur != -1 && chain++ < VPN_MAX_CHAIN && chainsUsed < VPN_MAX_CHAIN_TOTAL) {
                int d = i - cur;
                if (d > 32000) break;
                if (src[cur] != src[i]) { cur = prev[cur]; continue; }
                int l = 0;
                while (l < 255 && i + l < src_len && src[cur + l] == src[i + l]) l++;
                if (l > best_len && l >= 3) {
                    best_len = l;
                    best_dist = d;
                    if (best_len >= 128) break; // Good enough
                }
                cur = prev[cur];
            }
            chainsUsed++;
        }
        
        if (best_len >= 3) {
            // Match: 1-bit flag(1) + 8-bit length + 15-bit distance = 24 bits
            arshiva_write_bits(dst, &dst_len, 1, 1, &acc, &nB);
            arshiva_write_bits(dst, &dst_len, best_len, 8, &acc, &nB);
            arshiva_write_bits(dst, &dst_len, best_dist, 15, &acc, &nB);
            i += best_len;
        } else {
            // Literal: 1-bit flag(0) + 8-bit byte = 9 bits
            arshiva_write_bits(dst, &dst_len, 0, 1, &acc, &nB);
            arshiva_write_bits(dst, &dst_len, src[i], 8, &acc, &nB);
            i++;
        }
    }
    
    // Flush remaining bits
    if (nB > 0) {
        dst[dst_len++] = (uint8_t)(acc & 0xFF);
    }
    
    // Only return if strictly smaller
    if (dst_len < src_len) return dst_len;
    return 0; // No gain
}

// ==========================================
// ArShiva LZ77 Decompression
// ==========================================

// Decompresses src → dst. Returns original size, or -1 on error.
static inline int vpn_decompress(const uint8_t* src, int src_len, 
                                  uint8_t* dst, int dst_capacity) {
    if (src_len <= 0 || dst_capacity <= 0) return -1;
    
    uint32_t bitPtr = 0;
    uint32_t totalBits = (uint32_t)src_len * 8;
    int dst_len = 0;
    
    while (dst_len < dst_capacity && bitPtr < totalBits) {
        uint32_t flag = arshiva_read_bits(&bitPtr, src, src_len, 1);
        
        if (flag == 1) {
            // Match: 8-bit length + 15-bit distance
            uint32_t len  = arshiva_read_bits(&bitPtr, src, src_len, 8);
            uint32_t dist = arshiva_read_bits(&bitPtr, src, src_len, 15);
            
            if (len == 0 || dist == 0) break; // Invalid
            
            int srcIdx = dst_len - (int)dist;
            if (srcIdx < 0) break; // Invalid
            
            for (uint32_t k = 0; k < len && dst_len < dst_capacity; k++) {
                dst[dst_len] = dst[srcIdx + k];
                dst_len++;
            }
        } else {
            // Literal: 8-bit byte
            uint32_t b = arshiva_read_bits(&bitPtr, src, src_len, 8);
            dst[dst_len++] = (uint8_t)b;
        }
    }
    
    return dst_len;
}

// ==========================================
// BATCH BUFFER: Collects multiple TUN packets
// Format: [len1:2B][data1:len1B][len2:2B][data2:len2B]...
// ==========================================
struct VpnBatchBuffer {
    uint8_t  data[VPN_BATCH_MAX];
    int      used;        // Occupied bytes
    int      pkt_count;   // Number of collected packets
    
    void reset() {
        used = 0;
        pkt_count = 0;
    }
    
    // Adds a TUN packet to batch.
    // Returns true if packet was accepted.
    // Returns false if batch is full (call flush() first).
    bool add_packet(const uint8_t* pkt, int pkt_len) {
        if (pkt_len <= 0 || pkt_len > 1500) return false;
        // 2-byte length prefix + payload
        int needed = 2 + pkt_len;
        if (used + needed > VPN_BATCH_MAX) return false;
        if (pkt_count >= VPN_BATCH_MAX_PACKETS) return false;
        
        // Length as Big-Endian 16-bit
        data[used]     = (pkt_len >> 8) & 0xFF;
        data[used + 1] = pkt_len & 0xFF;
        memcpy(data + used + 2, pkt, pkt_len);
        used += needed;
        pkt_count++;
        return true;
    }
    
    // Is batch empty?
    bool is_empty() const { return used == 0; }
    
    // Is batch full enough to send?
    bool is_full() const {
        return used >= 1200 || pkt_count >= VPN_BATCH_MAX_PACKETS;
    }
};

// Extracts sub-packets from a decompressed batch.
// Callback is invoked for each sub-packet.
typedef void (*vpn_batch_callback)(const uint8_t* pkt, int pkt_len, void* ctx);

static inline int vpn_batch_extract(const uint8_t* batch_data, int batch_len,
                                     vpn_batch_callback cb, void* ctx) {
    int offset = 0;
    int count = 0;
    while (offset + 2 <= batch_len) {
        int pkt_len = (batch_data[offset] << 8) | batch_data[offset + 1];
        offset += 2;
        if (pkt_len <= 0 || offset + pkt_len > batch_len) break;
        cb(batch_data + offset, pkt_len, ctx);
        offset += pkt_len;
        count++;
    }
    return count;
}
