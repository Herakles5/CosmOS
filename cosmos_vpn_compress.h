#pragma once
// ==========================================
// COSMOS VPN KOMPRESSION – ArShiva LZ77 Hybrid
// Extrahiert aus ArShiva_fixed.cpp (BastionEngine)
// Komprimiert VOR Verschlüsselung, dekomprimiert NACH Entschlüsselung
// ==========================================
#include <string.h>
#include <stdint.h>

// Maximale Batch-Größe (für Batching: mehrere TUN-Pakete gesammelt)
// 16KB = ~10 TUN-Pakete bei MTU 1400 → weniger Encrypt/Header-Overhead
#define VPN_BATCH_MAX          16384
// Maximale Wartezeit zum Sammeln von Paketen (Millisekunden)
#define VPN_BATCH_TIMEOUT_MS   5
// Maximale Anzahl Sub-Pakete pro Batch
#define VPN_BATCH_MAX_PACKETS  64

// ArShiva VPN-Effort: Minimale Kompression für maximalen Durchsatz
// HTTPS-Traffic ist nicht komprimierbar – CPU sparen statt sinnlos komprimieren!
#define VPN_COMPRESS_EFFORT    1
// Maximaler Match-Chain pro Effort
#define VPN_MAX_CHAIN          (VPN_COMPRESS_EFFORT * 1000)
#define VPN_MAX_CHAIN_TOTAL    (VPN_COMPRESS_EFFORT * 500)

// Worst-Case: Unkomprimierbar → 1 Bit Flag + 8 Bit pro Byte = 9/8 Ratio + Padding
#define VPN_COMPRESS_BOUND(size) ((size) + ((size) / 8) + 2)

// ==========================================
// ArShiva Bit-Level I/O (aus BastionEngine)
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
// ArShiva LZ77 Kompression (VPN-optimiert)
// Effort auf Speed getrimmt für Echtzeit
// ==========================================

// Komprimiert src → dst. Gibt komprimierte Größe zurück, oder 0 bei Fehler/kein Gewinn.
static inline int vpn_compress(const uint8_t* src, int src_len, 
                                uint8_t* dst, int dst_capacity) {
    if (src_len <= 0 || dst_capacity <= 0) return 0;
    if (src_len < 4) {
        // Zu kurz zum Komprimieren – direkt kopieren
        if (dst_capacity < src_len) return 0;
        memcpy(dst, src, src_len);
        return src_len;
    }
    
    uint32_t acc = 0;
    int nB = 0;
    int dst_len = 0;
    
    // Hash-Chain Tabelle (16-Bit Hash → letzte Position)
    // Stack-allokiert für VPN-Paketgrößen (max ~4KB)
    int head[65536];
    memset(head, -1, sizeof(head));
    
    // Prev-Chain (für jede Position die vorherige mit gleichem Hash)
    // Max VPN_BATCH_MAX Einträge
    int prev[VPN_BATCH_MAX];
    memset(prev, -1, src_len < (int)sizeof(prev) ? src_len * sizeof(int) : sizeof(prev));
    
    // Hash-Chain aufbauen
    for (int k = 0; k + 2 < src_len; k++) {
        uint32_t h = ((uint32_t)src[k] << 8 | src[k+1]) ^ ((uint32_t)src[k+2] << 4);
        prev[k] = head[h & 0xFFFF];
        head[h & 0xFFFF] = k;
    }
    
    int i = 0;
    int chainsUsed = 0;
    
    while (i < src_len) {
        // Sicherheitscheck: Ist Output bereits größer als Input?
        if (dst_len >= src_len - 4) return 0; // Kein Gewinn
        
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
                    if (best_len >= 128) break; // Gut genug
                }
                cur = prev[cur];
            }
            chainsUsed++;
        }
        
        if (best_len >= 3) {
            // Match: 1-Bit Flag(1) + 8-Bit Länge + 15-Bit Distanz = 24 Bit
            arshiva_write_bits(dst, &dst_len, 1, 1, &acc, &nB);
            arshiva_write_bits(dst, &dst_len, best_len, 8, &acc, &nB);
            arshiva_write_bits(dst, &dst_len, best_dist, 15, &acc, &nB);
            i += best_len;
        } else {
            // Literal: 1-Bit Flag(0) + 8-Bit Byte = 9 Bit
            arshiva_write_bits(dst, &dst_len, 0, 1, &acc, &nB);
            arshiva_write_bits(dst, &dst_len, src[i], 8, &acc, &nB);
            i++;
        }
    }
    
    // Restbits flushen
    if (nB > 0) {
        dst[dst_len++] = (uint8_t)(acc & 0xFF);
    }
    
    // Nur zurückgeben wenn tatsächlich kleiner
    if (dst_len < src_len) return dst_len;
    return 0; // Kein Gewinn
}

// ==========================================
// ArShiva LZ77 Dekompression
// ==========================================

// Dekomprimiert src → dst. Gibt originale Größe zurück, oder -1 bei Fehler.
static inline int vpn_decompress(const uint8_t* src, int src_len, 
                                  uint8_t* dst, int dst_capacity) {
    if (src_len <= 0 || dst_capacity <= 0) return -1;
    
    uint32_t bitPtr = 0;
    uint32_t totalBits = (uint32_t)src_len * 8;
    int dst_len = 0;
    
    while (dst_len < dst_capacity && bitPtr < totalBits) {
        uint32_t flag = arshiva_read_bits(&bitPtr, src, src_len, 1);
        
        if (flag == 1) {
            // Match: 8-Bit Länge + 15-Bit Distanz
            uint32_t len  = arshiva_read_bits(&bitPtr, src, src_len, 8);
            uint32_t dist = arshiva_read_bits(&bitPtr, src, src_len, 15);
            
            if (len == 0 || dist == 0) break; // Ungültig
            
            int srcIdx = dst_len - (int)dist;
            if (srcIdx < 0) break; // Ungültig
            
            for (uint32_t k = 0; k < len && dst_len < dst_capacity; k++) {
                dst[dst_len] = dst[srcIdx + k];
                dst_len++;
            }
        } else {
            // Literal: 8-Bit Byte
            uint32_t b = arshiva_read_bits(&bitPtr, src, src_len, 8);
            dst[dst_len++] = (uint8_t)b;
        }
    }
    
    return dst_len;
}

// ==========================================
// BATCH-BUFFER: Sammelt mehrere TUN-Pakete
// Format: [len1:2B][data1:len1B][len2:2B][data2:len2B]...
// ==========================================
struct VpnBatchBuffer {
    uint8_t  data[VPN_BATCH_MAX];
    int      used;        // Belegte Bytes
    int      pkt_count;   // Anzahl gesammelter Pakete
    
    void reset() {
        used = 0;
        pkt_count = 0;
    }
    
    // Fügt ein TUN-Paket zum Batch hinzu.
    // Gibt true zurück wenn das Paket aufgenommen wurde.
    // Gibt false zurück wenn der Batch voll ist (erst flush() aufrufen).
    bool add_packet(const uint8_t* pkt, int pkt_len) {
        if (pkt_len <= 0 || pkt_len > 1500) return false;
        // 2 Byte Längen-Präfix + Payload
        int needed = 2 + pkt_len;
        if (used + needed > VPN_BATCH_MAX) return false;
        if (pkt_count >= VPN_BATCH_MAX_PACKETS) return false;
        
        // Länge als Big-Endian 16-Bit
        data[used]     = (pkt_len >> 8) & 0xFF;
        data[used + 1] = pkt_len & 0xFF;
        memcpy(data + used + 2, pkt, pkt_len);
        used += needed;
        pkt_count++;
        return true;
    }
    
    // Ist der Batch leer?
    bool is_empty() const { return used == 0; }
    
    // Ist der Batch voll genug zum Senden?
    bool is_full() const {
        return used >= 1200 || pkt_count >= VPN_BATCH_MAX_PACKETS;
    }
};

// Extrahiert Sub-Pakete aus einem dekomprimierten Batch.
// Callback wird für jedes Sub-Paket aufgerufen.
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
