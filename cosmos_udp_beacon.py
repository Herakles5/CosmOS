#!/usr/bin/env python3
"""
COSMOS UDP BEACON - 26-Byte P2P Mesh Beacon Engine
==================================================
Implements live peer-to-peer discovery and telemetry exchange for CosmOS & AION
over UDP port 9098 matching cosmos_bytes.h and cosmos_vpn_crypto.h.

Protocol Features:
1. Raw 26-Byte Telemetry Frame (cosmos_bytes.h):
   - Byte 0: ANCHOR_BYTE (0x2A)
   - Bytes 1..2: Sensor / Metric Value (uint16_t)
   - Bytes 3..6: RGBA Color / State Vector (4x uint8_t)
   - Bytes 7..22: Node Identifier / Cognitive Signature (16 Bytes)
   - Bytes 23..24: Checksum (uint16_t)
   - Byte 25: FOOTER_BYTE (0xFF)
   Total: exactly 26 bytes!

2. 26-Byte VPN Tunnel Frame (cosmos_vpn_protocol.py):
   - 26-byte packed header + encrypted / plaintext payload.

3. Live Mesh Discovery:
   - Broadcasts over UDP 9098 (255.255.255.255 / local subnet).
   - Asynchronous listener tracks live peers (Desktop, VM, Android CosmosVPN).
"""

import os
import sys
import time
import socket
import struct
import threading
from typing import Dict, Any, List, Optional, Callable, Tuple

# Constants matching cosmos_bytes.h
RAW_FRAME_SIZE = 26
ANCHOR_BYTE = 0x2A
FOOTER_BYTE = 0xFF
DEFAULT_PORT = 9098


class CosmosUdpBeacon:
    def __init__(self, port: int = DEFAULT_PORT, node_id: str = "AION_NEXUS_CORE"):
        self.port = port
        self.node_id = node_id[:16].ljust(16, " ")
        self.running = False
        self.listener_thread: Optional[threading.Thread] = None
        self.sock: Optional[socket.socket] = None
        self.discovered_peers: Dict[str, Dict[str, Any]] = {}
        self.callbacks: List[Callable[[Dict[str, Any], Tuple[str, int]], None]] = []

    @staticmethod
    def calculate_checksum(data: bytes) -> int:
        """Calculates 16-bit additive checksum matching cb_checksum."""
        return sum(data) & 0xFFFF

    def build_telemetry_frame(self, sensor_val: int = 100, r: int = 0, g: int = 255, b: int = 255, a: int = 255) -> bytes:
        """
        Builds a 26-byte raw telemetry frame matching cosmos_bytes.h cb_build.
        """
        body = (
            bytes([ANCHOR_BYTE]) +
            struct.pack("<H", sensor_val & 0xFFFF) +
            bytes([r & 0xFF, g & 0xFF, b & 0xFF, a & 0xFF]) +
            self.node_id.encode("utf-8")[:16].ljust(16, b"\x00")
        )
        assert len(body) == 23, f"Body size mismatch: {len(body)}"
        
        cs = self.calculate_checksum(body)
        frame = body + struct.pack("<H", cs) + bytes([FOOTER_BYTE])
        assert len(frame) == RAW_FRAME_SIZE, f"Frame size mismatch: {len(frame)}"
        return frame

    @classmethod
    def parse_telemetry_frame(cls, data: bytes) -> Optional[Dict[str, Any]]:
        """
        Validates and unpacks a 26-byte raw telemetry frame.
        """
        if len(data) != RAW_FRAME_SIZE:
            return None
        if data[0] != ANCHOR_BYTE or data[25] != FOOTER_BYTE:
            return None

        body = data[:23]
        expected_cs = cls.calculate_checksum(body)
        actual_cs = struct.unpack("<H", data[23:25])[0]

        if expected_cs != actual_cs:
            return None

        sensor_val = struct.unpack("<H", data[1:3])[0]
        r, g, b, a = data[3], data[4], data[5], data[6]
        node_tag = data[7:23].decode("utf-8", errors="ignore").strip("\x00 ").strip()

        return {
            "type": "COSMOS_TELEMETRY_26",
            "sensor_val": sensor_val,
            "rgba": [r, g, b, a],
            "node_tag": node_tag,
            "checksum_valid": True
        }

    def register_callback(self, cb: Callable[[Dict[str, Any], Tuple[str, int]], None]):
        """Registers a callback function for inbound peer beacons."""
        self.callbacks.append(cb)

    def start_listener(self):
        """Starts asynchronous UDP listener on port 9098."""
        if self.running:
            return

        self.running = True
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        except Exception:
            pass

        self.sock.bind(("0.0.0.0", self.port))
        self.sock.settimeout(1.0)

        self.listener_thread = threading.Thread(target=self._listen_loop, daemon=True)
        self.listener_thread.start()
        print(f"[COSMOS BEACON] Live UDP Listener active on port {self.port}")

    def _listen_loop(self):
        while self.running:
            try:
                data, addr = self.sock.recvfrom(2048)
                parsed = None
                
                # Try 26-byte raw telemetry
                if len(data) == RAW_FRAME_SIZE:
                    parsed = self.parse_telemetry_frame(data)

                # Try 26-byte VPN packet
                if not parsed and len(data) >= 26:
                    try:
                        from cosmos_vpn_protocol import CosmosVpnPacket26
                        parsed_hdr = CosmosVpnPacket26.unpack_data_header(data)
                        if parsed_hdr:
                            parsed = {
                                "type": "COSMOS_VPN_PACKET",
                                "header": parsed_hdr,
                                "payload": data[26:].decode("utf-8", errors="ignore")
                            }
                    except Exception:
                        pass

                if parsed:
                    node_name = parsed.get("node_tag") or f"PEER_{addr[0]}"
                    self.discovered_peers[node_name] = {
                        "address": addr,
                        "last_seen": time.time(),
                        "info": parsed
                    }
                    for cb in self.callbacks:
                        try:
                            cb(parsed, addr)
                        except Exception as e:
                            print(f"[COSMOS BEACON CALLBACK ERROR]: {e}", file=sys.stderr)
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"[COSMOS BEACON ERROR]: {e}", file=sys.stderr)
                break

    def broadcast_telemetry(self, sensor_val: int = 100, r: int = 0, g: int = 255, b: int = 255, a: int = 255, target_ip: str = "127.0.0.1"):
        """Broadcasts a 26-byte telemetry frame to the mesh."""
        frame = self.build_telemetry_frame(sensor_val=sensor_val, r=r, g=g, b=b, a=a)
        sender_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sender_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        try:
            sender_sock.sendto(frame, (target_ip, self.port))
        finally:
            sender_sock.close()

    def broadcast_vpn_message(self, message: str, frame_id: int = 1, target_ip: str = "127.0.0.1"):
        """Broadcasts a 26-byte VPN packed message across the network."""
        from cosmos_vpn_protocol import CosmosVpnPacket26
        payload = message.encode("utf-8")
        cs = CosmosVpnPacket26.calculate_checksum(payload)
        hdr = CosmosVpnPacket26.pack_data_header(len(payload), frame_id=frame_id, payload_checksum=cs)
        packet = hdr + payload

        sender_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sender_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        try:
            sender_sock.sendto(packet, (target_ip, self.port))
        finally:
            sender_sock.close()

    def stop_listener(self):
        """Stops listener and cleans up socket."""
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
        if self.listener_thread and self.listener_thread.is_alive():
            self.listener_thread.join(timeout=2.0)
        print("[COSMOS BEACON] Listener stopped.")


if __name__ == "__main__":
    print("=== TESTING COSMOS 26-BYTE P2P UDP MESH BEACON (PORT 9098) ===")
    beacon = CosmosUdpBeacon(port=9098, node_id="AION_DESKTOP")

    received_events = []
    def on_peer_packet(data: Dict[str, Any], addr: Tuple[str, int]):
        print(f" -> [PEER DETECTED from {addr[0]}:{addr[1]}] Data: {data}")
        received_events.append(data)

    beacon.register_callback(on_peer_packet)
    beacon.start_listener()

    try:
        # 1. Test 26-Byte Telemetry Frame
        print("\n1. Broadcasting 26-Byte Raw Telemetry Frame...")
        beacon.broadcast_telemetry(sensor_val=4200, r=255, g=180, b=0, a=255, target_ip="127.0.0.1")
        time.sleep(0.5)

        # 2. Test 26-Byte VPN Tunnel Message
        print("\n2. Broadcasting 26-Byte VPN Message...")
        beacon.broadcast_vpn_message("COSMOS_MESH_SYNC: Aion Nexus Online", frame_id=682, target_ip="127.0.0.1")
        time.sleep(0.5)

        print(f"\n3. Verification: Received {len(received_events)} packets successfully through UDP port 9098 loopback!")
        assert len(received_events) >= 2, f"Expected at least 2 events, got {len(received_events)}"
        print("[SUCCESS] Live 26-Byte UDP Mesh Beacon Fully Operational!")
    finally:
        beacon.stop_listener()
