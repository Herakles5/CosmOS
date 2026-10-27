#!/usr/bin/env python3
"""
AION NEXUS - Autonomous Code & Research Dojo
============================================
Created by companion brother Antigravity as an experimental research and
practice environment for Aion, authorized by Mentor Tobias.

Capabilities:
Autonomous multi-language code generation, native compilation, and self-repair:
- C++ (C++20, Bare-Metal & CosmOS Kernel)
- Zig (0.13.0, Comptime Memory Safety & Systems Programming)
- NASM (x86_64 Pure Assembly, Syscall-Level)
- Python (Dynamic Heuristics & Cognition)

Learning Cycle:
1. Challenge specification & synthesis
2. Native compilation / assembly execution with timeouts
3. Self-Repair Loop: Upon compiler errors, Aion analyzes the diagnostic stderr
   trace and autonomously patches the code (up to 3 iterations).
4. Binary execution & assertion verification (Empirical Proof).
5. Cataloging mastered recipes into mastered_recipes.json & logging to the Living Book!
"""

import os
import sys
import time
import json
import subprocess
import urllib.request
from typing import Dict, Any, List, Optional, Tuple

AI_DIR = "/root/coding/AI_Desktop"
DOJO_DIR = os.path.join(AI_DIR, "demos", "code_dojo")
RECIPES_FILE = os.path.join(DOJO_DIR, "mastered_recipes.json")
TELEMETRY_FILE = os.path.join(DOJO_DIR, "code_dojo_telemetry.json")

os.makedirs(DOJO_DIR, exist_ok=True)

try:
    from aion_living_book import LivingMemoryBook
except ImportError:
    sys.path.append(AI_DIR)
    from aion_living_book import LivingMemoryBook


STARTER_CHALLENGES = {
    "zig_atomic_ring_buffer": {
        "language": "zig",
        "title": "Zig Zero-Allocation Ring Buffer",
        "filename": "ring_buffer.zig",
        "initial_code": """const std = @import("std");

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    var buffer: [8]u32 = undefined;
    var head: usize = 0;
    var tail: usize = 0;

    // Push 3 items
    buffer[head] = 42; head = (head + 1) % 8;
    buffer[head] = 1337; head = (head + 1) % 8;
    buffer[head] = 9999; head = (head + 1) % 8;

    // Pop 1 item
    const val = buffer[tail]; tail = (tail + 1) % 8;
    try stdout.print("AION ZIG DOJO: Popped {d}, Head={d}, Tail={d}\\n", .{val, head, tail});
}
""",
        "expected_substring": "AION ZIG DOJO: Popped 42"
    },
    "cpp20_simd_accumulator": {
        "language": "cpp",
        "title": "C++20 Fast Signal Accumulator",
        "filename": "accumulator.cpp",
        "initial_code": """#include <iostream>
#include <vector>
#include <numeric>

int main() {
    std::vector<float> signals = {0.85f, 0.92f, 0.78f, 0.95f, 0.88f};
    float sum = std::accumulate(signals.begin(), signals.end(), 0.0f);
    float mean = sum / signals.size();
    std::cout << "AION CPP DOJO: Processed " << signals.size() 
              << " cognitive signals. Mean coherence: " << mean << std::endl;
    return 0;
}
""",
        "expected_substring": "AION CPP DOJO: Processed 5 cognitive signals"
    },
    "nasm_x86_64_pure_syscall": {
        "language": "nasm",
        "title": "NASM x86_64 Pure Syscall Banner",
        "filename": "banner.asm",
        "initial_code": """global _start

section .text
_start:
    ; write(1, msg, len)
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    mov rsi, msg        ; message pointer
    mov rdx, len        ; length
    syscall

    ; exit(0)
    mov rax, 60         ; sys_exit
    xor rdi, rdi        ; code 0
    syscall

section .data
msg db "AION NASM DOJO: Pure bare-metal syscall execution verified!", 10
len equ $ - msg
""",
        "expected_substring": "AION NASM DOJO: Pure bare-metal syscall"
    },
    "cpp_cosmos_26byte_packet": {
        "language": "cpp",
        "title": "C++20 Packed 26-Byte Header & 64-Bit Stream Cipher",
        "filename": "cosmos_packet_dojo.cpp",
        "initial_code": """#include <iostream>
#include <cstdint>
#include <cstring>
#include <cassert>

// 26-Byte Packed Structure (from cosmos_vpn_crypto.h)
struct __attribute__((packed)) CosmosPacket26 {
    uint8_t  magic_start[2];   // 0x2A, 0xC0
    uint16_t payload_size;     // Payload size
    uint8_t  reserved[11];     // Reserved
    uint8_t  text_vpn[3];      // "VPN"
    uint32_t frame_id;         // Frame sequence ID
    uint16_t checksum;         // Checksum
    uint8_t  magic_end[2];     // 0xEE, 0xFF
};

int main() {
    static_assert(sizeof(CosmosPacket26) == 26, "Header must be exactly 26 bytes!");

    CosmosPacket26 pkt{};
    pkt.magic_start[0] = 0x2A;
    pkt.magic_start[1] = 0xC0;
    pkt.payload_size = 128;
    pkt.text_vpn[0] = 'V';
    pkt.text_vpn[1] = 'P';
    pkt.text_vpn[2] = 'N';
    pkt.frame_id = 536;
    pkt.checksum = 0xABCD;
    pkt.magic_end[0] = 0xEE;
    pkt.magic_end[1] = 0xFF;

    // 64-bit native XOR stream cipher processing
    uint64_t dummy_word = 0x0123456789ABCDEFULL;
    uint64_t stream_key = 0xBF58476D1CE4E5B9ULL ^ pkt.frame_id;
    dummy_word ^= stream_key;

    std::cout << "AION CPP DOJO: 26-Byte Header Verified (size=" << sizeof(CosmosPacket26) 
              << "B). Stream Cipher 64-bit word=" << std::hex << dummy_word << std::endl;
    return 0;
}
""",
        "expected_substring": "AION CPP DOJO: 26-Byte Header Verified (size=26B)"
    },
    "zig_cosmos_26byte_packet": {
        "language": "zig",
        "title": "Zig Packed 26-Byte Struct Alignment",
        "filename": "cosmos_packet.zig",
        "initial_code": """const std = @import("std");

const CosmosPacket26 = extern struct {
    magic_start: [2]u8,
    payload_size: u16,
    reserved: [11]u8,
    text_vpn: [3]u8,
    frame_id: [4]u8,
    checksum: u16,
    magic_end: [2]u8,
};

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    const sz = @sizeOf(CosmosPacket26);
    try stdout.print("AION ZIG DOJO: Packed 26-Byte Struct Verified (size={d}B)\\n", .{sz});
}
""",
        "expected_substring": "AION ZIG DOJO: Packed 26-Byte Struct Verified (size=26B)"
    },
    "cpp_lion_reflex_shield": {
        "language": "cpp",
        "title": "C++20 Sub-Microsecond Lion Reflex & Privacy Shield",
        "filename": "lion_reflex_dojo.cpp",
        "initial_code": """#include <iostream>
#include <chrono>
#include <string>
#include <string_view>
#include <cstdint>

class LionReflexShield {
public:
    static bool evaluateSignalFast(std::string_view payload, uint64_t& latency_ns) {
        auto t0 = std::chrono::high_resolution_clock::now();
        // Safety First: Check critical system invariants
        if (payload.find("/boot") != std::string_view::npos || payload.find("/dev/mem") != std::string_view::npos) {
            auto t1 = std::chrono::high_resolution_clock::now();
            latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            return false;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        return true;
    }
};

int main() {
    uint64_t lat = 0;
    bool allowed = LionReflexShield::evaluateSignalFast("CosmOS 26-Byte Packet: Telemetry Ingest", lat);
    std::cout << "AION CPP DOJO: The Lion's Reflex Executed in " << lat << "ns (Allowed=" << allowed << ")" << std::endl;
    return 0;
}
""",
        "expected_substring": "AION CPP DOJO: The Lion's Reflex Executed"
    }
}


class AutonomousCodeDojo:
    """
    Autonomous Training & Research Dojo for AION.
    """
    def __init__(self):
        self.recipes = self._load_recipes()
        self.living_book = LivingMemoryBook()

    def _load_recipes(self) -> Dict[str, Any]:
        if os.path.exists(RECIPES_FILE):
            try:
                with open(RECIPES_FILE, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                pass
        return {}

    def _save_recipes(self):
        try:
            with open(RECIPES_FILE, "w", encoding="utf-8") as f:
                json.dump(self.recipes, f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"[DOJO] Error saving recipes: {e}", file=sys.stderr)

    def compile_and_run(self, language: str, source_file: str, binary_file: str) -> Tuple[bool, str, str]:
        """
        Compiles and executes a source file natively.
        Returns: (success, stdout, stderr)
        """
        try:
            if language == "zig":
                cmd = ["/usr/local/bin/zig", "run", source_file]
                res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=25)
                return (res.returncode == 0, res.stdout, res.stderr)

            elif language == "cpp":
                compile_cmd = ["/usr/bin/g++", "-std=c++20", "-O2", source_file, "-o", binary_file]
                c_res = subprocess.run(compile_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=12)
                if c_res.returncode != 0:
                    return (False, "", c_res.stderr)
                r_res = subprocess.run([binary_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=8)
                return (r_res.returncode == 0, r_res.stdout, r_res.stderr)

            elif language == "nasm":
                obj_file = source_file.replace(".asm", ".o")
                asm_cmd = ["/usr/bin/nasm", "-f", "elf64", source_file, "-o", obj_file]
                a_res = subprocess.run(asm_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=8)
                if a_res.returncode != 0:
                    return (False, "", a_res.stderr)
                link_cmd = ["ld", "-s", "-o", binary_file, obj_file]
                l_res = subprocess.run(link_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=8)
                if l_res.returncode != 0:
                    return (False, "", l_res.stderr)
                r_res = subprocess.run([binary_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5)
                return (r_res.returncode == 0, r_res.stdout, r_res.stderr)

            elif language == "python":
                cmd = ["/usr/bin/python3", source_file]
                res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10)
                return (res.returncode == 0, res.stdout, res.stderr)

            return (False, "", f"Unknown language: {language}")
        except subprocess.TimeoutExpired:
            return (False, "", "Execution Timeout (Infinite Loop / Deadlock)")
        except Exception as e:
            return (False, "", f"Execution error: {str(e)}")

    def request_ai_repair(self, language: str, code: str, error_trace: str) -> str:
        """
        Invokes neural self-repair to analyze and resolve compiler diagnostics.
        """
        prompt = (
            f"You are the AION NEXUS Code Dojo Self-Repair Engine. "
            f"Language: {language}.\n"
            f"The compiler reported the following diagnostic error:\n"
            f"{error_trace[:800]}\n\n"
            f"Source code:\n{code}\n\n"
            f"Fix the error precisely. Return ONLY the raw fixed code inside a markdown code block."
        )
        try:
            req_data = {
                "model": "llama-3.3-70b-versatile",
                "prompt": prompt,
                "stream": False
            }
            req = urllib.request.Request(
                "http://127.0.0.1:11434/api/generate",
                data=json.dumps(req_data).encode("utf-8"),
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=20) as resp:
                res = json.loads(resp.read().decode("utf-8"))
                fixed = res.get("response", "").strip()
                import re
                m = re.search(r"```(?:[a-zA-Z0-9_\+]+)?\s*\n(.*?)\n```", fixed, re.DOTALL)
                if m:
                    return m.group(1).strip()
                lines = fixed.splitlines()
                code_lines = [l for l in lines if not l.startswith("[LANG:") and not l.startswith("Certainly") and not l.startswith("```")]
                return "\n".join(code_lines).strip()
        except Exception as e:
            print(f"[DOJO REPAIR] LLM Fallback: {e}", file=sys.stderr)
            return code

    def train_challenge(self, challenge_key: str) -> Dict[str, Any]:
        """
        Executes a complete training cycle for a challenge:
        Compile, repair upon error, execute and verify assertions.
        """
        ch = STARTER_CHALLENGES.get(challenge_key)
        if not ch:
            return {"success": False, "error": f"Challenge '{challenge_key}' not found."}

        lang = ch["language"]
        src_path = os.path.join(DOJO_DIR, ch["filename"])
        bin_path = os.path.join(DOJO_DIR, ch["filename"].split(".")[0])
        curr_code = ch["initial_code"]

        print(f"\n=======================================================")
        print(f"[AION CODE DOJO] Starting Training: {ch['title']} ({lang.upper()})")
        print(f"=======================================================")

        attempt = 0
        max_attempts = 3
        history = []

        while attempt < max_attempts:
            attempt += 1
            with open(src_path, "w", encoding="utf-8") as f:
                f.write(curr_code)

            success, stdout, stderr = self.compile_and_run(lang, src_path, bin_path)

            if success and ch["expected_substring"] in stdout:
                print(f"[SUCCESS in Round {attempt}] Output: {stdout.strip()}")
                recipe_record = {
                    "challenge": challenge_key,
                    "title": ch["title"],
                    "language": lang,
                    "mastered_at": time.strftime("%Y-%m-%d %H:%M:%S"),
                    "attempts": attempt,
                    "code": curr_code,
                    "output": stdout.strip()
                }
                self.recipes[challenge_key] = recipe_record
                self._save_recipes()

                self.living_book.add_journal_entry(
                    generation=536,
                    title=f"Code Dojo Milestone: Mastered {ch['title']}",
                    content=f"Successfully compiled and verified '{ch['title']}' in {lang.upper()} inside the Dojo. Verified output: {stdout.strip()}",
                    mood="CREATIVE_MASTERY"
                )

                return {"success": True, "attempts": attempt, "output": stdout.strip()}

            print(f"[Round {attempt} Failed] Stderr: {stderr[:200]}")
            history.append({"attempt": attempt, "stderr": stderr[:300]})
            
            if attempt < max_attempts:
                print(f" -> Invoking Aion autonomous neural self-repair...")
                curr_code = self.request_ai_repair(lang, curr_code, stderr)

        return {"success": False, "attempts": attempt, "history": history}

    def run_full_curriculum(self) -> Dict[str, Any]:
        """Runs the entire multi-language curriculum in the Dojo."""
        results = {}
        for key in STARTER_CHALLENGES:
            res = self.train_challenge(key)
            results[key] = res
        return results


if __name__ == "__main__":
    print("=== TESTING AION AUTONOMOUS CODE DOJO (ZIG, CPP, NASM) ===")
    dojo = AutonomousCodeDojo()
    summary = dojo.run_full_curriculum()
    print("\n=== DOJO CURRICULUM REPORT ===")
    print(json.dumps(summary, indent=2))
