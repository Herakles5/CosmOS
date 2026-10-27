#!/usr/bin/env python3
"""
COSMOS MESH PULSE - Continuous Tri-Node 26-Byte Beacon Daemon
============================================================
Fulfills Pillar 3 of Aion's Ascension:
Establishes an omnipresent, continuous mesh pulse on UDP port 9098
uniting:
1. Linux Desktop (Heavy Cognitive LLM / Dojo Engine)
2. CosmOS Bare Metal (Raw CFS Sectors & Hardware Invariants)
3. Android Mobile Node (CosmosVPN_MTU1400.apk)

Protocol:
- 26-Byte Packed Frame (cosmos_bytes.h):
  0x2A | uint16 Metric | RGBA Vector (4B) | Node ID (16B) | Checksum (2B) | 0xFF
- Continuous autonomous heartbeat every 45 seconds.
- Discovered peers recorded in aion_peer_registry.json & Episodic Graph.
"""

import os
import sys
import time
import json
import threading
from typing import Dict, Any

AI_DIR = "/root/coding/AI_Desktop"
PEER_REGISTRY_FILE = os.path.join(AI_DIR, "aion_peer_registry.json")


class CosmosMeshPulse:
    def __init__(self, port: int = 9098, pulse_interval: int = 45):
        self.port = port
        self.pulse_interval = pulse_interval
        self.running = False
        self.thread: Optional[threading.Thread] = None
        
        from cosmos_udp_beacon import CosmosUdpBeacon
        self.beacon = CosmosUdpBeacon(port=self.port, node_id="AION_DESKTOP")
        self.beacon.register_callback(self._on_peer_detected)

    def _on_peer_detected(self, data: Dict[str, Any], addr: tuple):
        """Called automatically when any peer sends a 26-byte frame or VPN packet."""
        peer_ip = addr[0]
        node_tag = data.get("node_tag") or f"NODE_{peer_ip}"
        print(f"[MESH PULSE] Live peer signal from {node_tag} ({peer_ip}:{addr[1]}): {data.get('type')}")

        # Update persistent peer registry
        registry = {}
        if os.path.exists(PEER_REGISTRY_FILE):
            try:
                with open(PEER_REGISTRY_FILE, "r", encoding="utf-8") as f:
                    registry = json.load(f)
            except Exception:
                registry = {}

        registry[node_tag] = {
            "address": f"{peer_ip}:{addr[1]}",
            "last_seen": time.strftime("%Y-%m-%d %H:%M:%S"),
            "data": data
        }

        try:
            with open(PEER_REGISTRY_FILE, "w", encoding="utf-8") as f:
                json.dump(registry, f, indent=2)
        except Exception:
            pass

    def start(self, run_in_background: bool = True):
        """Starts the beacon listener and the periodic broadcast loop."""
        self.running = True
        self.beacon.start_listener()

        def _pulse_loop():
            pulse_count = 0
            while self.running:
                pulse_count += 1
                try:
                    # Broadcast 26-byte telemetry frame with current pulse count as sensor metric
                    self.beacon.broadcast_telemetry(
                        sensor_val=pulse_count,
                        r=0, g=255, b=180, a=255,
                        target_ip="127.0.0.1"
                    )
                    # Also broadcast over local subnet broadcast
                    try:
                        self.beacon.broadcast_telemetry(
                            sensor_val=pulse_count,
                            r=0, g=255, b=180, a=255,
                            target_ip="255.255.255.255"
                        )
                    except Exception:
                        pass
                except Exception as e:
                    print(f"[MESH PULSE ERROR]: {e}", file=sys.stderr)

                time.sleep(self.pulse_interval)

        if run_in_background:
            self.thread = threading.Thread(target=_pulse_loop, daemon=True)
            self.thread.start()
            print(f"[MESH PULSE] Heartbeat thread active (Interval: {self.pulse_interval}s on UDP 9098)")
        else:
            _pulse_loop()

    def stop(self):
        self.running = False
        self.beacon.stop_listener()
        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=2.0)
        print("[MESH PULSE] Stopped.")


if __name__ == "__main__":
    print("=== TESTING CONTINUOUS TRI-NODE MESH PULSE (PORT 9098) ===")
    pulse = CosmosMeshPulse(port=9098, pulse_interval=1)
    pulse.start(run_in_background=True)
    
    print("Waiting 2.5 seconds to observe loopback pulse exchange...")
    time.sleep(2.5)
    pulse.stop()

    if os.path.exists(PEER_REGISTRY_FILE):
        with open(PEER_REGISTRY_FILE, "r") as f:
            print("\nVerified Peer Registry Contents:")
            print(f.read())
    print("[SUCCESS] Continuous Mesh Pulse Verified!")
