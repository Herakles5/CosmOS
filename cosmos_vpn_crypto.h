#pragma once
#include "schneider_lang.h"
#include <string.h>

// Flag Definitions
#define FLAG_DATA_TUNNEL     0x00
#define FLAG_HANDSHAKE_INIT  0x01
#define FLAG_HANDSHAKE_REPLY 0x02

// 26-Byte Handshake Packet Structure
_202 cosmos_hs_packet {
    _184 magic_start[2];       // [0..1]: 0x2A, 0xC0
    _184 type_flag;            // [2]: Handshake-Typ (Init / Reply / Data)
    _89  client_nonce;         // [3..6]: 4-Byte Client Nonce
    _89  server_nonce;         // [7..10]: 4-Byte Server Nonce
    _184 padding[13];          // [11..23]: Auffüllung
    _184 magic_end[2];         // [24..25]: 0xEE, 0xFF
} __attribute__((packed));

// 26-Byte Data Packet Header Structure (for reference/casting)
_202 cosmos_data_header {
    _184 magic_start[2];       // [0..1]: 0x2A, 0xC0
    _182 payload_size;         // [2..3]: Payload Size (Notice overlapping with type_flag, but data packets have non-zero size, while type_flag=1/2 means size=0 if seen as big-endian)
                               // Actually, for data tunnel, packet[2..3] is payload size.
                               // In handshake, packet[2] is type_flag, packet[3..6] is nonce.
                               // This means payload_size in handshake would look like (type_flag << 8) | packet[3].
                               // We'll handle this safely in the code.
    _184 reserved[11];         // [4..14]
    _184 text_vpn[3];          // [15..17]: "VPN"
    _89  frame_id;             // [18..21]: Frame ID (used for stream cipher)
    _182 checksum;             // [22..23]: Payload Checksum
    _184 magic_end[2];         // [24..25]: 0xEE, 0xFF
} __attribute__((packed));

// Key Derivation: Generates an 8-byte session key from 4-byte nonces
_189 _50 compute_session_key(_89 c_nonce, _89 s_nonce, _184* out_key) {
    // A more complex mix than just *31 ^ *37
    _94 mix1 = (_94)c_nonce * 0x9E3779B185EBCA87ULL;
    _94 mix2 = (_94)s_nonce * 0xC2B2AE3D27D4EB4FULL;
    
    _94 combined = mix1 ^ ((mix2 << 13) | (mix2 >> 51));
    combined ^= (combined >> 33);
    combined *= 0xFF51AFD7ED558CCDULL;
    combined ^= (combined >> 33);
    
    for (_43 i = 0; i < 8; i++) {
        out_key[i] = (_184)(combined & 0xFF);
        combined >>= 8;
    }
}

// Simple Payload Checksum
_189 _182 calculate_checksum(_71 _184* data, _89 len) {
    _182 sum = 0;
    for (_89 i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// XOR Stream Cipher that incorporates the frame_id to prevent trivial replays/analysis
// 64-Bit optimiert: Verarbeitet 8 Bytes auf einmal statt Byte-für-Byte
_189 _50 encrypt_decrypt_payload(_184* payload, _89 len, _71 _184* session_key, _89 frame_id) {
    // Expand frame_id into a pseudo-random 8-byte modifier
    _94 frame_mix = (_94)frame_id * 0xBF58476D1CE4E5B9ULL;
    frame_mix ^= (frame_mix >> 27);
    
    // Build 64-bit stream key from session_key XOR frame modifier
    _94 key64;
    {
        _184 stream_key[8];
        for (_43 i = 0; i < 8; i++) {
            stream_key[i] = session_key[i] ^ (_184)(frame_mix & 0xFF);
            frame_mix >>= 8;
        }
        memcpy(&key64, stream_key, 8);
    }
    
    // Process 8 bytes at a time using 64-bit XOR
    _89 words = len / 8;
    _89 remainder = len % 8;
    _94* p64 = (_94*)payload;
    
    for (_89 i = 0; i < words; i++) {
        p64[i] ^= key64;
        // Pure stream cipher cascade (independent of payload data to guarantee reversibility)
        key64 += (key64 >> 13) ^ ((_94)i * 0x0101010101010101ULL);
    }
    
    // Handle remaining bytes (0-7) individually
    _15 (remainder > 0) {
        _184* tail = payload + words * 8;
        _184 key_bytes[8];
        memcpy(key_bytes, &key64, 8);
        for (_89 i = 0; i < remainder; i++) {
            tail[i] ^= key_bytes[i];
            key_bytes[i] += (key_bytes[i] >> 3) ^ ((words * 8 + i) & 0xFF);
        }
    }
}
