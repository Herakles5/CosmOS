#!/usr/bin/env python3
# ==============================================================================
# AION NEXUS — AUTONOMOUS SELF-LEARNING RL AGENT V5 (AION 2)
# ==============================================================================
# Sovereign AI agent that autonomously plays and learns AION 2 using:
#   1. V5 RL Combat Brain: Q(lambda) eligibility traces, terminal kill credit
#      assignment, UCB1 action exploration, 2-step combo synergy priors,
#      action-masked cooldown awareness, adaptive skill cooldown discovery,
#      and mini-batch experience replay.
#   2. Vectorized NumPy X11 Vision: Single-ROI BGRA grabs for HP/MP/Target,
#      death-debounce filtering, 3D nameplate tracking, and directional
#      top-right minimap radar steering.
#   3. Zero-Fork XTest Motor: Persistent X11 keyboard/mouse injection (with
#      xdotool fallback), active-window safety guard, camera turn steering,
#      complete post-kill approach/loot/step-back state machine.
#   4. Self-Healing Quest & Farm Navigation: Unreachable-target breakout,
#      radar-guided scouting, and non-destructive Episodic Graph / IPC sync.
#   5. GUI & Hotkey Compatibility: Full support for aion2_gaming_gui.py,
#      /tmp/aion_nexus_status.json, /tmp/aion_nexus_cmd.txt, [F8] & [F10].
# ==============================================================================

import json
import math
import os
import random
import re
import socket
import subprocess
import sys
import time
from collections import deque
from typing import Any, Deque, Dict, Iterable, List, Optional, Tuple

import numpy as np
from Xlib import X, XK, display
from Xlib.ext import xtest

AI_DIR = "/root/coding/AI_Desktop"
MEINOS_DIR = "/root/coding/MeinOS"
if AI_DIR not in sys.path:
    sys.path.append(AI_DIR)
if MEINOS_DIR not in sys.path:
    sys.path.append(MEINOS_DIR)

try:
    from aion_episodic_graph import EpisodicKnowledgeGraph
except ImportError:
    EpisodicKnowledgeGraph = None

AION_WINDOW_TITLES = ("AION2", "AION 2", "GeForce NOW")
COMBAT_MEMORY_FILE = os.path.join(MEINOS_DIR, "aion_nexus_combat_memory.json")
STATUS_FILE = "/tmp/aion_nexus_status.json"
CMD_FILE = "/tmp/aion_nexus_cmd.txt"
LOG_FILE = "/tmp/aion_nexus_log.txt"
MAX_LOG_BYTES = 2 * 1024 * 1024

# Normalized HUD Ratios (Resolution & Window Independent)
NORM_HP_X_START = 0.3844
NORM_HP_X_END = 0.4813
NORM_HP_Y = 0.9028

NORM_MP_X_START = 0.5182
NORM_MP_X_END = 0.6130
NORM_MP_Y = 0.9055

NORM_TGT_X_START = 0.4234
NORM_TGT_X_END = 0.5850
NORM_TGT_Y_CENTER = 0.0620

NORM_MINIMAP_X_START = 0.86
NORM_MINIMAP_X_END = 0.98
NORM_MINIMAP_Y_START = 0.05
NORM_MINIMAP_Y_END = 0.22

# Game Hotkeys
KEY_TARGET = "Tab"
KEY_SKILL_1 = "1"
KEY_SKILL_2 = "2"
KEY_SKILL_3 = "3"
KEY_SKILL_4 = "4"
KEY_SKILL_5 = "5"
KEY_SKILL_Q = "q"
KEY_SKILL_E = "e"
KEY_SKILL_R = "r"
KEY_LOOT = "f"
KEY_INTERACT = "f"
KEY_DIALOGUE_ADVANCE = "space"
KEY_POTION_HP = "7"
KEY_POTION_MP = "8"
KEY_FORWARD = "w"
KEY_BACKWARD = "s"
KEY_LEFT = "a"
KEY_RIGHT = "d"
KEY_JUMP = "space"

ACTIONS = [
    "L_AUTO_SHOT",
    "R_HEAVY_SHOT",
    "SKILL_1",
    "SKILL_2",
    "SKILL_3",
    "SKILL_4",
    "SKILL_5",
    "SKILL_Q",
    "SKILL_E",
    "SKILL_R",
    "TACTICAL_KITE",
    "STRAFE_LEFT",
    "STRAFE_RIGHT",
    "JUMP_UNSTUCK",
]

SKILL_KEY_MAP: Dict[str, str] = {
    "SKILL_1": KEY_SKILL_1,
    "SKILL_2": KEY_SKILL_2,
    "SKILL_3": KEY_SKILL_3,
    "SKILL_4": KEY_SKILL_4,
    "SKILL_5": KEY_SKILL_5,
    "SKILL_Q": KEY_SKILL_Q,
    "SKILL_E": KEY_SKILL_E,
    "SKILL_R": KEY_SKILL_R,
}

DEFAULT_SKILL_COOLDOWNS: Dict[str, float] = {
    "SKILL_1": 1.5,
    "SKILL_2": 3.0,
    "SKILL_3": 5.0,
    "SKILL_4": 7.5,
    "SKILL_5": 9.0,
    "SKILL_Q": 4.0,
    "SKILL_E": 6.0,
    "SKILL_R": 12.0,
}

OFFENSIVE_SKILLS = set(SKILL_KEY_MAP.keys()) | {"L_AUTO_SHOT", "R_HEAVY_SHOT"}
MOVEMENT_SKILLS = {"TACTICAL_KITE", "STRAFE_LEFT", "STRAFE_RIGHT", "JUMP_UNSTUCK"}


class AionLogger:
    """Thread-safe rotating session logger for GUI telemetry console."""

    @staticmethod
    def log(msg: str):
        formatted = f"[{time.strftime('%H:%M:%S')}] {msg}\n"
        try:
            if os.path.exists(LOG_FILE) and os.path.getsize(LOG_FILE) > MAX_LOG_BYTES:
                rotated = LOG_FILE + ".1"
                os.replace(LOG_FILE, rotated)
            with open(LOG_FILE, "a", encoding="utf-8") as f:
                f.write(formatted)
        except Exception:
            pass


class AionSpeechBridge:
    """Rate-limited bridge to /tmp/ide_to_aion.txt for spoken narration."""

    def __init__(self):
        self.bridge_pipe = "/tmp/ide_to_aion.txt"
        self.last_spoken = 0.0
        self.cooldown = 3.5
        self.last_thought = ""

    def speak(self, text: str, force: bool = False):
        now = time.time()
        if not force and (now - self.last_spoken) < self.cooldown:
            return
        self.last_spoken = now
        self.last_thought = text
        formatted = f"[AION] {text}\n"
        print(f"\n\033[95m{formatted.strip()}\033[0m")
        AionLogger.log(f"[AION SPEECH] {text}")
        try:
            with open(self.bridge_pipe, "a", encoding="utf-8") as f:
                f.write(formatted)
        except Exception:
            pass


class DynamicWindowGeometry:
    """Tracks live AION 2 / GeForce NOW X11 window bounds and focus state."""

    def __init__(self, win_titles: Iterable[str] = AION_WINDOW_TITLES):
        self.win_titles = tuple(win_titles)
        self.wid: Optional[int] = None
        self.matched_title: str = ""
        self.ax = 0
        self.ay = 0
        self.w = 1920
        self.h = 1080
        self.last_check = 0.0
        self.update(force=True)

    def update(self, force: bool = False) -> bool:
        now = time.time()
        if not force and (now - self.last_check) < 2.0 and self.wid is not None:
            return True
        self.last_check = now
        for title in self.win_titles:
            try:
                out = subprocess.check_output(
                    ["xdotool", "search", "--name", title],
                    text=True,
                    stderr=subprocess.DEVNULL,
                    timeout=1.0,
                ).strip()
                if not out:
                    continue
                for line in out.splitlines():
                    cand_wid = int(line.strip())
                    winfo = subprocess.check_output(
                        ["xwininfo", "-id", str(cand_wid)],
                        text=True,
                        stderr=subprocess.DEVNULL,
                        timeout=1.0,
                    )
                    mw = re.search(r"Width:\s*(\d+)", winfo)
                    mh = re.search(r"Height:\s*(\d+)", winfo)
                    mx = re.search(r"Absolute upper-left X:\s*(-?\d+)", winfo)
                    my = re.search(r"Absolute upper-left Y:\s*(-?\d+)", winfo)
                    if not (mw and mh and mx and my):
                        continue
                    w, h = int(mw.group(1)), int(mh.group(1))
                    # Skip hidden 1x1 Chromium helper windows
                    if w >= 320 and h >= 240:
                        self.wid = cand_wid
                        self.matched_title = title
                        self.ax = int(mx.group(1))
                        self.ay = int(my.group(1))
                        self.w = w
                        self.h = h
                        return True
            except Exception:
                continue
        self.wid = None
        return False

    def is_game_active(self) -> bool:
        """Returns True only when a real game window exists and is focused (or user isn't on GUI)."""
        if not self.wid:
            return False
        try:
            active = subprocess.check_output(
                ["xdotool", "getactivewindow"],
                text=True,
                stderr=subprocess.DEVNULL,
                timeout=0.5,
            ).strip()
            return bool(active) and int(active) == int(self.wid)
        except Exception:
            return False


class AionVision:
    """Vectorized NumPy X11 perception with temporal debouncing & radar steering."""

    def __init__(self, geom: DynamicWindowGeometry, disp: Optional[display.Display] = None):
        self.geom = geom
        self.disp = disp
        self.root = disp.screen().root if disp is not None else None
        self._hp_ema: Optional[float] = None
        self._mp_ema: Optional[float] = None

    def _grab_bgra(self, x: int, y: int, w: int, h: int) -> Optional[np.ndarray]:
        if self.root is None or w <= 0 or h <= 0:
            return None
        try:
            img = self.root.get_image(x, y, w, h, X.ZPixmap, 0xFFFFFFFF)
            arr = np.frombuffer(img.data, dtype=np.uint8)
            expected = w * h * 4
            if arr.size < expected:
                return None
            return arr[:expected].reshape((h, w, 4))
        except Exception:
            return None

    @staticmethod
    def parse_hp_bgra(bgra: np.ndarray) -> float:
        """Extracts percentage [0..100] from BGRX HP bar strip using column coverage."""
        if bgra is None or bgra.size == 0:
            return 0.0
        b = bgra[:, :, 0]
        g = bgra[:, :, 1]
        r = bgra[:, :, 2]
        # Tolerates compressed stream shifts while remaining red-dominant
        mask = (r > 115) & (g < 80) & (b < 80) & (r > g.astype(np.int16) + 45)
        # Measure horizontal bar fill via columns with >=1 matching pixel
        col_fill = np.any(mask, axis=0)
        return round(min(100.0, float(col_fill.mean()) * 100.0), 1)

    @staticmethod
    def parse_mp_bgra(bgra: np.ndarray) -> float:
        """Extracts percentage [0..100] from BGRX MP bar strip."""
        if bgra is None or bgra.size == 0:
            return 0.0
        b = bgra[:, :, 0]
        g = bgra[:, :, 1]
        r = bgra[:, :, 2]
        mask = (b > 115) & (r < 85) & (b > r.astype(np.int16) + 40)
        col_fill = np.any(mask, axis=0)
        return round(min(100.0, float(col_fill.mean()) * 100.0), 1)

    @staticmethod
    def parse_target_bgra(bgra: np.ndarray) -> Optional[float]:
        """Detects active target red bar and returns HP percentage, or None if no target."""
        if bgra is None or bgra.size == 0:
            return None
        b = bgra[:, :, 0]
        g = bgra[:, :, 1]
        r = bgra[:, :, 2]
        mask = (r > 135) & (g < 80) & (b < 80) & (r > g.astype(np.int16) + 55)
        row_counts = mask.sum(axis=1)
        best_row_red = int(row_counts.max()) if row_counts.size else 0
        if best_row_red < 12:
            return None
        col_fill = np.any(mask, axis=0)
        pct = float(col_fill.mean()) * 100.0
        return round(max(1.0, min(100.0, pct)), 1)

    @staticmethod
    def parse_viewport_enemies_bgra(bgra: np.ndarray, vx_offset: int = 0, vy_offset: int = 0) -> List[Dict[str, Any]]:
        """Single-frame vectorized clustering of overhead enemy nameplates & HP bars."""
        if bgra is None or bgra.size == 0:
            return []
        sampled = bgra[::4, :, :]
        b = sampled[:, :, 0]
        g = sampled[:, :, 1]
        r = sampled[:, :, 2]
        yellow = (r > 180) & (g > 155) & (b < 115)
        red = (r > 135) & (g < 70) & (b < 70)
        combined = yellow | red
        row_hits = combined.sum(axis=1)
        h_s, w_s = combined.shape
        center_x = w_s / 2.0

        detected: List[Dict[str, Any]] = []
        for r_idx in np.where(row_hits >= 6)[0]:
            xs = np.where(combined[r_idx])[0]
            cx_local = int(xs.mean())
            cx = vx_offset + cx_local
            y = vy_offset + int(r_idx * 4)
            if not any(abs(cx - d["x"]) < 48 and abs(y - d["y"]) < 36 for d in detected):
                dx_norm = round((cx_local - center_x) / max(1.0, center_x), 3)
                detected.append({"x": cx, "y": y, "dx_norm": dx_norm, "type": "enemy_in_sight"})
        return detected

    @staticmethod
    def parse_minimap_radar_bgra(bgra: np.ndarray) -> Dict[str, Any]:
        """Splits minimap red enemy blips into left/front/right sectors for guided scouting."""
        if bgra is None or bgra.size == 0:
            return {"total": 0, "left": 0, "front": 0, "right": 0, "steer_dir": 0}
        b = bgra[::2, ::2, 0]
        g = bgra[::2, ::2, 1]
        r = bgra[::2, ::2, 2]
        red = (r > 180) & (g < 70) & (b < 70)
        total = int(red.sum())
        if total == 0:
            return {"total": 0, "left": 0, "front": 0, "right": 0, "steer_dir": 0}
        h_m, w_m = red.shape
        third = max(1, w_m // 3)
        left_c = int(red[:, :third].sum())
        front_c = int(red[:max(1, h_m // 2), third:2 * third].sum())
        right_c = int(red[:, 2 * third:].sum())
        steer = -1 if left_c > max(right_c, front_c) else (1 if right_c > max(left_c, front_c) else 0)
        return {"total": total, "left": left_c, "front": front_c, "right": right_c, "steer_dir": steer}

    def get_player_hp(self) -> float:
        self.geom.update()
        if not self.geom.wid:
            return 0.0
        hp_x = self.geom.ax + int(NORM_HP_X_START * self.geom.w)
        hp_w = max(10, int((NORM_HP_X_END - NORM_HP_X_START) * self.geom.w))
        hp_y = self.geom.ay + int(NORM_HP_Y * self.geom.h)
        raw = self.parse_hp_bgra(self._grab_bgra(hp_x, hp_y, hp_w, 4))
        self._hp_ema = raw if self._hp_ema is None else round(0.75 * raw + 0.25 * self._hp_ema, 1)
        return self._hp_ema

    def get_player_mp(self) -> float:
        self.geom.update()
        if not self.geom.wid:
            return 0.0
        mp_x = self.geom.ax + int(NORM_MP_X_START * self.geom.w)
        mp_w = max(10, int((NORM_MP_X_END - NORM_MP_X_START) * self.geom.w))
        mp_y = self.geom.ay + int(NORM_MP_Y * self.geom.h)
        raw = self.parse_mp_bgra(self._grab_bgra(mp_x, mp_y, mp_w, 3))
        self._mp_ema = raw if self._mp_ema is None else round(0.75 * raw + 0.25 * self._mp_ema, 1)
        return self._mp_ema

    def get_target_hp(self) -> Optional[float]:
        self.geom.update()
        if not self.geom.wid:
            return None
        tgt_x = self.geom.ax + int(NORM_TGT_X_START * self.geom.w)
        tgt_w = max(20, int((NORM_TGT_X_END - NORM_TGT_X_START) * self.geom.w))
        tgt_y_top = max(0, self.geom.ay + int(NORM_TGT_Y_CENTER * self.geom.h) - 4)
        return self.parse_target_bgra(self._grab_bgra(tgt_x, tgt_y_top, tgt_w, 9))

    def detect_enemies_in_view(self) -> List[Dict[str, Any]]:
        self.geom.update()
        if not self.geom.wid:
            return []
        vx = self.geom.ax + int(0.20 * self.geom.w)
        vy = self.geom.ay + int(0.25 * self.geom.h)
        vw = max(20, int(0.60 * self.geom.w))
        vh = max(20, int(0.45 * self.geom.h))
        return self.parse_viewport_enemies_bgra(self._grab_bgra(vx, vy, vw, vh), vx, vy)

    def scan_minimap_enemies(self) -> Dict[str, Any]:
        self.geom.update()
        if not self.geom.wid:
            return {"total": 0, "left": 0, "front": 0, "right": 0, "steer_dir": 0}
        mx = self.geom.ax + int(NORM_MINIMAP_X_START * self.geom.w)
        my = self.geom.ay + int(NORM_MINIMAP_Y_START * self.geom.h)
        mw = max(16, int((NORM_MINIMAP_X_END - NORM_MINIMAP_X_START) * self.geom.w))
        mh = max(16, int((NORM_MINIMAP_Y_END - NORM_MINIMAP_Y_START) * self.geom.h))
        return self.parse_minimap_radar_bgra(self._grab_bgra(mx, my, mw, mh))


class AionMotor:
    """Zero-fork XTest input dispatcher with active-window focus safety."""

    def __init__(self, geom: DynamicWindowGeometry, disp: Optional[display.Display] = None, dry_run: bool = False):
        self.geom = geom
        self.disp = disp
        self.dry_run = dry_run
        self._keycodes: Dict[str, int] = {}
        self.actions_dispatched = 0
        if self.disp is not None:
            for key_name in {
                KEY_TARGET, KEY_SKILL_1, KEY_SKILL_2, KEY_SKILL_3, KEY_SKILL_4,
                KEY_SKILL_5, KEY_SKILL_Q, KEY_SKILL_E, KEY_SKILL_R, KEY_LOOT,
                KEY_DIALOGUE_ADVANCE, KEY_POTION_HP, KEY_POTION_MP,
                KEY_FORWARD, KEY_BACKWARD, KEY_LEFT, KEY_RIGHT, "Alt_L",
            }:
                self._resolve_keycode(key_name)

    def _resolve_keycode(self, key_name: str) -> Optional[int]:
        if key_name in self._keycodes:
            return self._keycodes[key_name]
        if self.disp is None:
            return None
        ks = XK.string_to_keysym(key_name)
        if ks == 0 and len(key_name) == 1:
            ks = ord(key_name)
        kc = self.disp.keysym_to_keycode(ks)
        if kc:
            self._keycodes[key_name] = kc
            return kc
        return None

    def _can_send_input(self) -> bool:
        if self.dry_run:
            return False
        return bool(self.geom.wid) and self.geom.is_game_active()

    def _send_key_event(self, key_name: str, pressed: bool):
        self.actions_dispatched += 1
        if not self._can_send_input():
            return
        kc = self._resolve_keycode(key_name)
        if kc and self.disp is not None:
            try:
                xtest.fake_input(self.disp, X.KeyPress if pressed else X.KeyRelease, kc)
                self.disp.sync()
                return
            except Exception:
                pass
        cmd = "keydown" if pressed else "keyup"
        subprocess.run(["xdotool", cmd, key_name], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _send_button_event(self, button: int, pressed: bool):
        self.actions_dispatched += 1
        if not self._can_send_input():
            return
        if self.disp is not None:
            try:
                xtest.fake_input(self.disp, X.ButtonPress if pressed else X.ButtonRelease, button)
                self.disp.sync()
                return
            except Exception:
                pass
        cmd = "mousedown" if pressed else "mouseup"
        subprocess.run(["xdotool", cmd, str(button)], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def focus_game(self) -> bool:
        if self.dry_run:
            return True
        if not self.geom.wid:
            self.geom.update(force=True)
        if not self.geom.wid:
            return False
        try:
            active = subprocess.check_output(
                ["xdotool", "getactivewindow"], text=True, stderr=subprocess.DEVNULL, timeout=0.5
            ).strip()
            if active:
                if int(active) == int(self.geom.wid):
                    return True
                active_name = subprocess.check_output(
                    ["xdotool", "getwindowname", active], text=True, stderr=subprocess.DEVNULL, timeout=0.5
                ).strip()
                if "AION 2 — NEURAL" in active_name or "NEURAL GAMING" in active_name:
                    return False
            subprocess.run(
                ["xdotool", "windowactivate", str(self.geom.wid)],
                check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            return True
        except Exception:
            return False

    def press_key(self, key: str, hold_ms: int = 45):
        self._send_key_event(key, True)
        if hold_ms > 0:
            time.sleep(hold_ms / 1000.0)
        self._send_key_event(key, False)

    def rapid_shoot_left(self, count: int = 3, delay: float = 0.05):
        for _ in range(count):
            self._send_button_event(1, True)
            time.sleep(0.015)
            self._send_button_event(1, False)
            if delay > 0:
                time.sleep(delay)

    def shoot_right(self, hold_sec: float = 0.08):
        self._send_button_event(3, True)
        time.sleep(hold_sec)
        self._send_button_event(3, False)

    def _move_and_shoot(self, key: str, duration: float = 0.30, shots: int = 3):
        self._send_key_event(key, True)
        if shots > 0:
            per_shot = max(0.02, duration / max(1, shots))
            self.rapid_shoot_left(count=shots, delay=per_shot)
        else:
            time.sleep(duration)
        self._send_key_event(key, False)

    def strafe_left_and_shoot(self, duration: float = 0.30):
        self._move_and_shoot(KEY_LEFT, duration=duration, shots=3)

    def strafe_right_and_shoot(self, duration: float = 0.30):
        self._move_and_shoot(KEY_RIGHT, duration=duration, shots=3)

    def kite_back_and_shoot(self, duration: float = 0.32):
        self._move_and_shoot(KEY_BACKWARD, duration=duration, shots=3)

    def step_back(self, duration: float = 0.25):
        """Steps backward briefly without firing (fixed missing method from V4)."""
        self._move_and_shoot(KEY_BACKWARD, duration=duration, shots=0)

    def walk_forward(self, duration: float = 0.5):
        self._move_and_shoot(KEY_FORWARD, duration=duration, shots=0)

    def jump_shot_unstuck(self):
        self._send_key_event(KEY_FORWARD, True)
        self.press_key(KEY_JUMP, hold_ms=40)
        self.rapid_shoot_left(count=2, delay=0.05)
        time.sleep(0.15)
        self._send_key_event(KEY_FORWARD, False)

    def steer_toward(self, steer_dir: float, duration: float = 0.22):
        """Steers character/camera gently left (<0) or right (>0) toward visual or radar targets."""
        if abs(steer_dir) < 0.08:
            return
        if self._can_send_input() and self.disp is not None:
            dx = int(max(-160, min(160, steer_dir * 120)))
            try:
                subprocess.run(
                    ["xdotool", "mousemove_relative", "--", str(dx), "0"],
                    check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                )
            except Exception:
                pass
        key = KEY_LEFT if steer_dir < 0 else KEY_RIGHT
        self._move_and_shoot(key, duration=duration, shots=0)

    def stop_movement(self):
        for k in (KEY_FORWARD, KEY_BACKWARD, KEY_LEFT, KEY_RIGHT, "Alt_L"):
            self._send_key_event(k, False)

    def loot(self):
        for _ in range(4):
            self.press_key(KEY_LOOT, hold_ms=50)
            time.sleep(0.10)

    def auto_run_quest(self):
        self.focus_game()
        self._send_key_event("Alt_L", True)
        self.press_key("1", hold_ms=40)
        self._send_key_event("Alt_L", False)
        time.sleep(0.05)

    def click_quest_tracker(self):
        self.auto_run_quest()

    def interact_npc(self):
        self.press_key(KEY_INTERACT, hold_ms=60)
        time.sleep(0.12)
        self.press_key(KEY_DIALOGUE_ADVANCE, hold_ms=45)


class GlobalHotkeyManager:
    """Polls X11 keymap cleanly for [F8] Mode Toggle and [F10]/[Pause] Pause."""

    def __init__(self, disp: Optional[display.Display] = None):
        self.disp = disp
        self.pause_keycodes = [76, 127, 75]
        self.mode_keycodes = [74]
        self.prev_pause_down = False
        self.prev_mode_down = False
        self.paused = False
        self.mode = "QUEST"

    def _poll_combo(self, keycodes: List[int], prev_attr: str) -> bool:
        if self.disp is None:
            return False
        try:
            keymap = self.disp.query_keymap()
            is_down = any(bool(keymap[kc // 8] & (1 << (kc % 8))) for kc in keycodes)
            prev_down = getattr(self, prev_attr)
            if is_down and not prev_down:
                setattr(self, prev_attr, True)
                return True
            if not is_down:
                setattr(self, prev_attr, False)
        except Exception:
            pass
        return False

    def check_pause_toggle(self) -> bool:
        if self._poll_combo(self.pause_keycodes, "prev_pause_down"):
            self.paused = not self.paused
            return True
        return False

    def check_mode_toggle(self) -> bool:
        if self._poll_combo(self.mode_keycodes, "prev_mode_down"):
            self.mode = "FARM" if self.mode == "QUEST" else "QUEST"
            return True
        return False


class NexusCombatBrain:
    """
    V5 Self-Learning Combat Brain:
      - Q(lambda) eligibility traces with terminal kill propagation
      - UCB1 exploration bonus + Action Masking (cooldowns & MP gated)
      - Empirical per-skill DPS & adaptive cooldown calibration
      - 2-step combo synergy matrix + bounded experience replay
    """

    MODEL_VERSION = "MULTI_DOMAIN_REINFORCEMENT_POLICY_V5_Q_LAMBDA_UCB"

    def __init__(self, memory_file: str = COMBAT_MEMORY_FILE):
        self.memory_file = memory_file
        self.alpha = 0.16
        self.gamma = 0.88
        self.lambda_trace = 0.72
        self.epsilon = 0.22
        self.epsilon_min = 0.05
        self.epsilon_decay = 0.992
        self.ucb_c = 0.75

        self.q_table: Dict[str, Dict[str, float]] = {}
        self.sa_counts: Dict[str, Dict[str, int]] = {}
        self.combo_q: Dict[str, Dict[str, float]] = {}
        self.skill_stats: Dict[str, Dict[str, Any]] = self._default_skill_stats()
        self.learned_cooldowns: Dict[str, float] = dict(DEFAULT_SKILL_COOLDOWNS)

        self.traces: Dict[Tuple[str, str], float] = {}
        self.replay_buffer: Deque[Tuple[str, str, float, str, bool]] = deque(maxlen=400)
        self.last_action: Optional[str] = None

        self.total_kills = 0
        self.total_damage_dealt = 0.0
        self.total_damage_taken = 0.0
        self.combat_durations: List[float] = []

        self._load_memory()

    @staticmethod
    def _default_skill_stats() -> Dict[str, Dict[str, Any]]:
        return {
            a: {"uses": 0, "hits": 0, "total_dmg": 0.0, "avg_dmg": 0.0, "zero_dmg_streak": 0}
            for a in ACTIONS
        }

    def _ensure_state(self, state: str):
        if state not in self.q_table:
            self.q_table[state] = {a: 0.0 for a in ACTIONS}
        else:
            for a in ACTIONS:
                self.q_table[state].setdefault(a, 0.0)
        if state not in self.sa_counts:
            self.sa_counts[state] = {a: 0 for a in ACTIONS}
        else:
            for a in ACTIONS:
                self.sa_counts[state].setdefault(a, 0)

    def _state_key(
        self,
        hp: float,
        mp: float,
        target_hp: Optional[float],
        in_melee: bool,
        enemy_count: int = 1,
        stalled: bool = False,
    ) -> str:
        hp_band = "high" if hp > 60 else ("mid" if hp > 35 else "low")
        mp_band = "high" if mp > 60 else ("mid" if mp > 20 else "low")
        if target_hp is None:
            tgt_band = "none"
        elif target_hp > 65:
            tgt_band = "full"
        elif target_hp > 30:
            tgt_band = "mid"
        else:
            tgt_band = "low"
        dist_band = "melee" if in_melee else "range"
        pack_tag = "multi" if enemy_count > 1 else "single"
        stall_tag = "_stall" if stalled else ""
        return f"{tgt_band}_{mp_band}_{hp_band}_{dist_band}_{pack_tag}{stall_tag}"

    def available_actions(
        self,
        mp: float,
        skill_last_used: Optional[Dict[str, float]] = None,
        now: Optional[float] = None,
        is_stuck: bool = False,
    ) -> List[str]:
        """Masks unready cooldown skills and low-MP heavy shots so learning stays accurate."""
        if is_stuck:
            return ["JUMP_UNSTUCK", "STRAFE_LEFT", "STRAFE_RIGHT"]

        now_ts = time.time() if now is None else now
        avail: List[str] = []
        for a in ACTIONS:
            if a == "R_HEAVY_SHOT" and mp < 20.0:
                continue
            if a in SKILL_KEY_MAP and skill_last_used is not None:
                last_t = skill_last_used.get(a, 0.0)
                cd = self.learned_cooldowns.get(a, DEFAULT_SKILL_COOLDOWNS.get(a, 2.0))
                if (now_ts - last_t) < cd:
                    continue
            avail.append(a)
        return avail or ["L_AUTO_SHOT"]

    def choose_action(
        self,
        state: str,
        mp: float,
        is_stuck: bool = False,
        skill_last_used: Optional[Dict[str, float]] = None,
        now: Optional[float] = None,
    ) -> str:
        self._ensure_state(state)
        candidates = self.available_actions(mp, skill_last_used=skill_last_used, now=now, is_stuck=is_stuck)

        if random.random() < self.epsilon:
            return random.choice(candidates)

        q_row = self.q_table[state]
        c_row = self.sa_counts[state]
        total_visits = sum(c_row.values()) + 1
        combo_row = self.combo_q.get(self.last_action or "", {})
        q_scale = max(1.0, max((abs(v) for v in q_row.values()), default=0.0))

        def _score(act: str) -> float:
            q_val = q_row.get(act, 0.0)
            visits = c_row.get(act, 0)
            ucb = self.ucb_c * q_scale * math.sqrt(math.log(total_visits + 1.0) / (visits + 1.0))
            combo = 0.25 * combo_row.get(act, 0.0)
            dmg_prior = 0.10 * min(20.0, self.skill_stats.get(act, {}).get("avg_dmg", 0.0))
            # Optimistic bonus for untried offensive skills so strong bursts are discovered systematically
            untried_bonus = (0.35 * q_scale) if (visits == 0 and act in OFFENSIVE_SKILLS) else 0.0
            return q_val + ucb + combo + dmg_prior + untried_bonus

        return max(candidates, key=_score)

    def record_skill_outcome(self, action: str, dmg_dealt: float):
        """Adapts learned skill damage average and effective cooldown when a ready skill whiffs."""
        stats = self.skill_stats.setdefault(
            action, {"uses": 0, "hits": 0, "total_dmg": 0.0, "avg_dmg": 0.0, "zero_dmg_streak": 0}
        )
        stats["uses"] += 1
        if dmg_dealt > 0.2:
            stats["hits"] += 1
            stats["total_dmg"] = round(stats["total_dmg"] + dmg_dealt, 2)
            stats["avg_dmg"] = round(0.8 * stats["avg_dmg"] + 0.2 * dmg_dealt if stats["avg_dmg"] > 0 else dmg_dealt, 2)
            stats["zero_dmg_streak"] = 0
            if action in self.learned_cooldowns:
                base_cd = DEFAULT_SKILL_COOLDOWNS[action]
                self.learned_cooldowns[action] = round(max(base_cd * 0.75, self.learned_cooldowns[action] - 0.15), 2)
        elif action in SKILL_KEY_MAP:
            stats["zero_dmg_streak"] += 1
            if stats["zero_dmg_streak"] >= 2:
                self.learned_cooldowns[action] = round(min(20.0, self.learned_cooldowns[action] + 0.5), 2)
                stats["zero_dmg_streak"] = 0

    def learn(
        self,
        state: str,
        action: str,
        reward: float,
        next_state: str,
        is_terminal: bool = False,
    ):
        self._ensure_state(state)
        self._ensure_state(next_state)

        self.sa_counts[state][action] = self.sa_counts[state].get(action, 0) + 1

        if self.last_action and action != self.last_action:
            prev_c = self.combo_q.setdefault(self.last_action, {})
            old_c = prev_c.get(action, 0.0)
            prev_c[action] = round(old_c + self.alpha * (reward - old_c), 4)
        self.last_action = action

        next_best = 0.0 if is_terminal else max(self.q_table[next_state].values(), default=0.0)
        td_error = reward + self.gamma * next_best - self.q_table[state][action]

        # Replacing traces across encounter so full combo gets terminal kill credit
        self.traces[(state, action)] = 1.0
        dead_traces: List[Tuple[str, str]] = []
        for (ts, ta), e_val in list(self.traces.items()):
            self._ensure_state(ts)
            self.q_table[ts][ta] = round(self.q_table[ts][ta] + self.alpha * td_error * e_val, 4)
            new_e = self.gamma * self.lambda_trace * e_val
            if new_e < 0.04:
                dead_traces.append((ts, ta))
            else:
                self.traces[(ts, ta)] = new_e
        for key in dead_traces:
            self.traces.pop(key, None)

        self.replay_buffer.append((state, action, reward, next_state, is_terminal))
        if is_terminal:
            self.traces.clear()
            self.last_action = None

    def replay_step(self, batch_size: int = 8):
        """Mini-batch off-policy Q replay between fights to stabilize policy convergence."""
        if len(self.replay_buffer) < batch_size:
            return
        for s, a, r, ns, term in random.sample(list(self.replay_buffer), batch_size):
            self._ensure_state(s)
            self._ensure_state(ns)
            target = r if term else (r + self.gamma * max(self.q_table[ns].values(), default=0.0))
            old_q = self.q_table[s][a]
            self.q_table[s][a] = round(old_q + (0.5 * self.alpha) * (target - old_q), 4)

    def decay_exploration(self):
        if len(self.combat_durations) >= 10:
            recent_ttk = sum(self.combat_durations[-5:]) / 5.0
            prior_ttk = sum(self.combat_durations[-10:-5]) / 5.0
            if recent_ttk > prior_ttk * 1.15 and self.epsilon < 0.22:
                self.epsilon = round(min(0.25, self.epsilon + 0.015), 4)
                return
        if self.epsilon > self.epsilon_min:
            self.epsilon = round(max(self.epsilon_min, self.epsilon * self.epsilon_decay), 4)

    def _load_memory(self):
        if not os.path.exists(self.memory_file):
            return
        try:
            with open(self.memory_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            prev_model = data.get("model", "")
            raw_q = data.get("q_table", {})

            if prev_model != self.MODEL_VERSION and raw_q:
                # Preserve V4 backup cleanly and soften stale JUMP/crash-loop skew on migration
                bak_path = self.memory_file.replace(".json", ".v4_backup.json")
                if not os.path.exists(bak_path):
                    with open(bak_path, "w", encoding="utf-8") as bf:
                        json.dump(data, bf, indent=2, ensure_ascii=False)
                self.q_table = {
                    st: {act: round(val * (0.15 if act == "JUMP_UNSTUCK" else 0.35), 4) for act, val in acts.items()}
                    for st, acts in raw_q.items()
                }
                self.epsilon = max(0.16, min(0.25, float(data.get("epsilon", 0.22))))
                self.combat_durations = [d for d in data.get("combat_durations", []) if 0.5 <= d <= 60.0][-25:]
            else:
                self.q_table = raw_q
                self.sa_counts = data.get("sa_counts", {})
                self.combo_q = data.get("combo_q", {})
                if data.get("skill_stats"):
                    self.skill_stats.update(data["skill_stats"])
                if data.get("learned_cooldowns"):
                    self.learned_cooldowns.update(data["learned_cooldowns"])
                self.epsilon = float(data.get("epsilon", 0.22))
                self.combat_durations = data.get("combat_durations", [])

            self.total_kills = int(data.get("total_kills", 0))
            self.total_damage_dealt = float(data.get("total_damage_dealt", 0.0))
            self.total_damage_taken = float(data.get("total_damage_taken", 0.0))
        except Exception:
            pass

    def save_memory(self):
        data = {
            "agent": "AION_NEXUS_AUTONOMOUS_GAMER",
            "model": self.MODEL_VERSION,
            "last_updated": time.strftime("%Y-%m-%d %H:%M:%S"),
            "total_kills": self.total_kills,
            "total_damage_dealt": round(self.total_damage_dealt, 1),
            "total_damage_taken": round(self.total_damage_taken, 1),
            "epsilon": self.epsilon,
            "states_learned": len(self.q_table),
            "combat_durations": self.combat_durations[-50:],
            "learned_cooldowns": self.learned_cooldowns,
            "skill_stats": self.skill_stats,
            "combo_q": self.combo_q,
            "sa_counts": self.sa_counts,
            "q_table": self.q_table,
        }
        tmp_path = self.memory_file + ".tmp"
        try:
            with open(tmp_path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            os.replace(tmp_path, self.memory_file)
        except Exception:
            pass


class AionNexusAutonomousGamer:
    """Autonomous AION 2 RL Gamer with Quest/Farm navigation and Evolution integration."""

    def __init__(self, initial_mode: str = "QUEST", dry_run: bool = False):
        self.dry_run = dry_run
        self.disp = None if dry_run else display.Display(":0")
        self.geom = DynamicWindowGeometry()
        self.vision = AionVision(self.geom, disp=self.disp)
        self.motor = AionMotor(self.geom, disp=self.disp, dry_run=dry_run)
        self.speech = AionSpeechBridge()
        self.brain = NexusCombatBrain()
        self.hotkeys = GlobalHotkeyManager(disp=self.disp)
        self.hotkeys.mode = initial_mode
        self.running = True

        self.kg = EpisodicKnowledgeGraph() if (EpisodicKnowledgeGraph and not dry_run) else None

        self.last_hp = 100.0
        self.last_mp = 100.0
        self.last_target_hp: Optional[float] = None
        self.combat_start = 0.0
        self.roam_count = 0
        self.kills = self.brain.total_kills
        self.missing_target_streak = 0
        self.no_dmg_streak = 0

        self.quest_autorun_active = False
        self.last_quest_alt1 = 0.0
        self.last_npc_interact = 0.0
        self.last_status_write = 0.0
        self.last_tab_scan = 0.0
        self.last_target_change = time.time()
        self.last_observed_target_hp: Optional[float] = None

        self.skill_last_used: Dict[str, float] = {k: 0.0 for k in SKILL_KEY_MAP}

    def _check_ipc_commands(self):
        if not os.path.exists(CMD_FILE):
            return
        try:
            with open(CMD_FILE, "r", encoding="utf-8") as f:
                cmd = f.read().strip()
            if not cmd:
                return
            with open(CMD_FILE, "w", encoding="utf-8") as f:
                f.write("")

            AionLogger.log(f"[IPC COMMAND RECEIVED] {cmd}")
            if cmd == "PAUSE":
                self.hotkeys.paused = True
                self.motor.stop_movement()
                self.speech.speak("Aion paused! Manual control engaged.", force=True)
            elif cmd == "RESUME":
                self.hotkeys.paused = False
                self.speech.speak(f"Aion resuming! Mode: {self.hotkeys.mode}", force=True)
            elif cmd == "TOGGLE_PAUSE":
                self.hotkeys.paused = not self.hotkeys.paused
                if self.hotkeys.paused:
                    self.motor.stop_movement()
                    self.speech.speak("Aion paused! You have manual control.", force=True)
                else:
                    self.speech.speak(f"Aion resuming! Mode: {self.hotkeys.mode}", force=True)
            elif cmd in {"MODE_QUEST", "QUEST_AUTORUN", "ALT_1", "CLICK_QUEST"}:
                self.hotkeys.mode = "QUEST"
                self.motor.auto_run_quest()
                self.quest_autorun_active = True
                self.last_quest_alt1 = time.time()
                self.speech.speak("Quest Auto-Run active with Alt+1! Running path to objective.", force=True)
                AionLogger.log("[QUEST] Alt+1 triggered! In-game Auto-Pathing engaged.")
            elif cmd == "MODE_FARM":
                self.hotkeys.mode = "FARM"
                self.quest_autorun_active = False
                self.speech.speak("Farm Mode activated!", force=True)
            elif cmd == "TOGGLE_MODE":
                self.hotkeys.mode = "FARM" if self.hotkeys.mode == "QUEST" else "QUEST"
                if self.hotkeys.mode == "QUEST":
                    self.motor.auto_run_quest()
                    self.quest_autorun_active = True
                    self.last_quest_alt1 = time.time()
                    self.speech.speak("Switched to Quest Mode: Alt+1 engaged!", force=True)
                else:
                    self.quest_autorun_active = False
                    self.speech.speak("Switched to Farm Mode!", force=True)
            elif cmd == "UNSTUCK":
                self.speech.speak("Unstuck initiated! Evading collision.", force=True)
                self.motor.jump_shot_unstuck()
            elif cmd == "TARGET":
                self.motor.press_key(KEY_TARGET)
            elif cmd == "INTERACT_NPC":
                self.motor.interact_npc()
            elif cmd == "STOP":
                self.running = False
        except Exception:
            pass

    def _write_status_telemetry(
        self,
        hp: float,
        mp: float,
        target_hp: Optional[float],
        current_action: str,
        reward: float,
        force: bool = False,
    ):
        now = time.time()
        if not force and (now - self.last_status_write) < 0.25:
            return
        self.last_status_write = now

        avg_ttk = sum(self.brain.combat_durations[-10:]) / max(1, len(self.brain.combat_durations[-10:]))
        payload = {
            "running": self.running,
            "paused": self.hotkeys.paused,
            "mode": self.hotkeys.mode,
            "window_attached": self.geom.wid is not None,
            "player_hp": round(hp, 1),
            "player_mp": round(mp, 1),
            "target_hp": round(target_hp, 1) if target_hp is not None else None,
            "kills": self.kills,
            "avg_ttk": round(avg_ttk, 1),
            "epsilon": round(self.brain.epsilon, 3),
            "states_learned": len(self.brain.q_table),
            "last_action": current_action,
            "last_reward": round(reward, 2),
            "last_thought": self.speech.last_thought,
            "timestamp": now,
        }
        tmp_status = STATUS_FILE + ".tmp"
        try:
            with open(tmp_status, "w", encoding="utf-8") as f:
                json.dump(payload, f, indent=2)
            os.replace(tmp_status, STATUS_FILE)
        except Exception:
            pass

    def execute_action(self, action: str):
        """Data-driven dispatch covering primary fire, cooldown skills, and kiting."""
        now = time.time()
        if action == "L_AUTO_SHOT":
            self.motor.rapid_shoot_left(count=3, delay=0.05)
        elif action == "R_HEAVY_SHOT":
            self.motor.shoot_right()
            self.motor.rapid_shoot_left(count=1, delay=0.04)
        elif action in SKILL_KEY_MAP:
            cd = self.brain.learned_cooldowns.get(action, DEFAULT_SKILL_COOLDOWNS[action])
            if (now - self.skill_last_used.get(action, 0.0)) >= cd:
                self.motor.press_key(SKILL_KEY_MAP[action], hold_ms=50)
                self.skill_last_used[action] = now
            self.motor.rapid_shoot_left(count=2, delay=0.045)
        elif action == "TACTICAL_KITE":
            self.motor.kite_back_and_shoot(duration=0.30)
        elif action == "STRAFE_LEFT":
            self.motor.strafe_left_and_shoot(duration=0.28)
        elif action == "STRAFE_RIGHT":
            self.motor.strafe_right_and_shoot(duration=0.28)
        elif action == "JUMP_UNSTUCK":
            self.motor.jump_shot_unstuck()

    def calculate_reward(
        self,
        prev_hp: float,
        curr_hp: float,
        prev_mp: float,
        curr_mp: float,
        prev_tgt: Optional[float],
        curr_tgt: Optional[float],
        action: str,
        is_stuck: bool = False,
        duration_sec: float = 0.0,
    ) -> float:
        reward = 0.0

        if prev_tgt is not None and curr_tgt is not None:
            dmg_dealt = prev_tgt - curr_tgt
            if dmg_dealt > 0:
                reward += dmg_dealt * 0.45
                self.brain.total_damage_dealt += dmg_dealt
            elif action in OFFENSIVE_SKILLS and is_stuck:
                reward -= 1.5

        if prev_tgt is not None and curr_tgt is None:
            reward += 25.0
            if 0.4 < duration_sec < 8.0:
                reward += round(12.0 * (8.0 - duration_sec) / 8.0, 2)

        dmg_taken = prev_hp - curr_hp
        if dmg_taken > 0:
            reward -= dmg_taken * 0.6
            self.brain.total_damage_taken += dmg_taken
            if action in {"TACTICAL_KITE", "STRAFE_LEFT", "STRAFE_RIGHT"}:
                reward += 1.5

        if curr_mp < 15.0 and action == "R_HEAVY_SHOT":
            reward -= 3.0

        if curr_hp < 55.0 and action in {"TACTICAL_KITE", "STRAFE_LEFT", "STRAFE_RIGHT"}:
            reward += 3.0

        if is_stuck:
            reward += 4.0 if action in MOVEMENT_SKILLS else -3.5

        return round(reward, 3)

    def record_evolution_milestone(self, kills: int, avg_ttk: float):
        if self.kg:
            try:
                node_id = f"aion2_mastery_kill_{kills}"
                self.kg.add_node(
                    node_id,
                    "EVOLUTIONARY_MILESTONE",
                    f"AION 2 RL Combat Mastery: {kills} Kills (Avg TTK {avg_ttk:.1f}s)",
                    generation=kills,
                    importance=0.92,
                )
                self.kg.add_or_reinforce_edge("tobias", node_id, "MENTORED_GAMING_BREAKTHROUGH", kills)
            except Exception:
                pass
        try:
            if os.path.exists("/tmp/aion_nexus.sock"):
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                    s.settimeout(0.3)
                    s.connect("/tmp/aion_nexus.sock")
                    msg = json.dumps({
                        "cmd": "INJECT_TASK",
                        "task": f"Analyze AION 2 RL combat mastery milestone ({kills} kills, avg TTK {avg_ttk:.1f}s, epsilon {self.brain.epsilon:.3f})",
                    }) + "\n"
                    s.sendall(msg.encode("utf-8"))
                    s.shutdown(socket.SHUT_WR)
        except Exception:
            pass

    def _finalize_kill(self, hp: float, mp: float, current_action: str) -> float:
        """Handles terminal RL update, replay, loot sprint, and guaranteed state reset."""
        duration = max(0.3, time.time() - self.combat_start)
        reward = self.calculate_reward(
            self.last_hp, hp, self.last_mp, mp, self.last_target_hp, None,
            current_action, duration_sec=duration,
        )
        prev_state = self.brain._state_key(self.last_hp, self.last_mp, self.last_target_hp, False)
        next_state = self.brain._state_key(hp, mp, None, False)

        try:
            self.kills += 1
            self.brain.total_kills = self.kills
            self.brain.combat_durations.append(round(duration, 2))
            self.brain.learn(prev_state, current_action, reward, next_state, is_terminal=True)
            self.brain.replay_step(batch_size=8)
            self.brain.decay_exploration()
            self.brain.save_memory()

            avg_ttk = sum(self.brain.combat_durations[-10:]) / max(1, len(self.brain.combat_durations[-10:]))
            log_msg = f"Kill #{self.kills} defeated in {duration:.1f}s (Avg TTK: {avg_ttk:.1f}s) | Q(lambda) Terminal: {reward:+.1f}"
            print(f"\n[AION NEXUS] {log_msg}")
            AionLogger.log(log_msg)

            if self.kills % 3 == 0:
                self.speech.speak(f"Kill number {self.kills}! Average TTK: {avg_ttk:.1f} seconds.")
                self.record_evolution_milestone(self.kills, avg_ttk)

            AionLogger.log("[LOOTING] Enemy defeated! Advancing to collect loot ([F])...")
            self.motor.walk_forward(duration=0.75)
            self.motor.loot()
            self.motor.step_back(duration=0.25)
        finally:
            # Always clear combat markers so no exception can ever trigger a fake kill loop
            self.combat_start = 0.0
            self.last_target_hp = None
            self.last_observed_target_hp = None
            self.missing_target_streak = 0
            self.no_dmg_streak = 0

        return reward

    def _acquire_target_guided(self) -> Optional[float]:
        """Combines Tab-targeting, 3D nameplate steering, and minimap radar bearing."""
        self.motor.press_key(KEY_TARGET)
        time.sleep(0.12)
        tgt = self.vision.get_target_hp()
        if tgt is not None:
            return tgt

        visible = self.vision.detect_enemies_in_view()
        if visible:
            closest = min(visible, key=lambda d: abs(d.get("dx_norm", 0.0)))
            self.motor.steer_toward(closest.get("dx_norm", 0.0), duration=0.18)
            self.motor.press_key(KEY_TARGET)
            time.sleep(0.12)
            tgt = self.vision.get_target_hp()
            if tgt is not None:
                return tgt

        radar = self.vision.scan_minimap_enemies()
        if radar["total"] > 0 and radar["steer_dir"] != 0:
            self.motor.steer_toward(float(radar["steer_dir"]) * 0.7, duration=0.22)
        return None

    def run(self):
        self.motor.focus_game()
        time.sleep(0.25)

        self.speech.speak(f"Aion Nexus V5 Q(lambda) agent active! Mode: {self.hotkeys.mode}.", force=True)
        AionLogger.log(f"=== AION NEXUS V5 SESSION STARTED (Mode: {self.hotkeys.mode}) ===")

        print("\n" + "=" * 80)
        print("  AION NEXUS V5 — SELF-LEARNING RL AGENT (AION 2)")
        print(f"  Mode: {self.hotkeys.mode} | Policy: Q(lambda) + UCB1 + Adaptive Cooldowns")
        print(f"  Window Attached: {self.geom.matched_title or 'WAITING'} | Kills: {self.brain.total_kills} | Q-States: {len(self.brain.q_table)}")
        print("  [F8] Toggle Mode | [F10/Pause] Global Pause")
        print("=" * 80 + "\n")

        if self.hotkeys.mode == "QUEST":
            self.motor.auto_run_quest()
            self.quest_autorun_active = True
            self.last_quest_alt1 = time.time()

        current_action = "L_AUTO_SHOT"
        reward = 0.0

        while self.running:
            try:
                self._check_ipc_commands()

                if self.hotkeys.check_pause_toggle():
                    if self.hotkeys.paused:
                        self.motor.stop_movement()
                        self.speech.speak("Aion paused! Manual control engaged.", force=True)
                    else:
                        self.speech.speak(f"Aion resuming! Mode: {self.hotkeys.mode}", force=True)

                if self.hotkeys.check_mode_toggle():
                    self.speech.speak(f"Mode switched to {self.hotkeys.mode}!", force=True)
                    AionLogger.log(f"[MODE TOGGLE] New Mode: {self.hotkeys.mode}")

                self.geom.update()
                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()
                self._write_status_telemetry(hp, mp, target_hp, current_action, reward)

                if self.hotkeys.paused or not self.geom.wid or not self.geom.is_game_active():
                    time.sleep(0.20)
                    continue

                if 0.0 < hp < 40.0:
                    self.motor.press_key(KEY_POTION_HP, hold_ms=60)
                    time.sleep(0.10)
                if 0.0 < mp < 20.0:
                    self.motor.press_key(KEY_POTION_MP, hold_ms=60)
                    time.sleep(0.10)

                if target_hp is None:
                    if self.combat_start > 0:
                        self.missing_target_streak += 1
                        # Two-frame debounce prevents transient UI flicker from ending combat early
                        if self.missing_target_streak < 2:
                            time.sleep(0.06)
                            continue
                        reward = self._finalize_kill(hp, mp, current_action)
                        chain = self._acquire_target_guided()
                        if chain is not None:
                            AionLogger.log(f"[CHAIN TARGET] Locked next mob! HP: {chain:.1f}%")
                            continue
                        if self.hotkeys.mode == "QUEST":
                            self.motor.auto_run_quest()
                            self.quest_autorun_active = True
                            self.last_quest_alt1 = time.time()

                    now = time.time()
                    if self.hotkeys.mode == "QUEST":
                        if not self.quest_autorun_active or (now - self.last_quest_alt1) > 20.0:
                            self.motor.auto_run_quest()
                            self.quest_autorun_active = True
                            self.last_quest_alt1 = now
                        if (now - self.last_npc_interact) > 1.8:
                            self.motor.interact_npc()
                            self.last_npc_interact = now
                        if (now - self.last_tab_scan) > 0.45:
                            self.last_tab_scan = now
                            scanned = self._acquire_target_guided()
                            if scanned is not None:
                                self.quest_autorun_active = False
                                continue
                        time.sleep(0.15)
                    else:
                        scanned = self._acquire_target_guided()
                        if scanned is not None:
                            continue
                        self.roam_count += 1
                        if self.roam_count >= 3:
                            radar = self.vision.scan_minimap_enemies()
                            if radar["steer_dir"] != 0:
                                self.motor.steer_toward(float(radar["steer_dir"]) * 0.6, duration=0.25)
                            else:
                                self.motor.press_key(random.choice([KEY_LEFT, KEY_RIGHT]), hold_ms=50)
                            self.motor.walk_forward(duration=0.50)
                            self.roam_count = 0
                            time.sleep(0.12)
                else:
                    self.missing_target_streak = 0
                    self.roam_count = 0
                    now = time.time()
                    if self.combat_start == 0.0:
                        self.combat_start = now
                        self.no_dmg_streak = 0
                        self.last_target_change = now
                        self.last_observed_target_hp = target_hp
                        self.speech.speak("Target locked! Engaging combat.")
                        AionLogger.log(f"[COMBAT START] Target locked! HP: {target_hp:.1f}%")

                    if self.last_observed_target_hp is None or abs(target_hp - self.last_observed_target_hp) > 1.5:
                        self.last_target_change = now
                        self.last_observed_target_hp = target_hp
                        self.no_dmg_streak = 0

                    stall_elapsed = now - self.last_target_change
                    is_stuck = (stall_elapsed > 6.5) or (self.no_dmg_streak >= 4)

                    if stall_elapsed > 12.0:
                        # Unreachable mob: escape and re-acquire instead of fighting a wall forever
                        AionLogger.log("[UNSTUCK] Target unreachable >12s — jumping and switching target.")
                        self.motor.jump_shot_unstuck()
                        self.motor.press_key(KEY_TARGET)
                        self.last_target_change = time.time()
                        self.no_dmg_streak = 0
                        continue

                    enemy_count = max(1, len(self.vision.detect_enemies_in_view()))
                    in_melee = (now - self.combat_start) > 2.2 or (self.last_hp - hp) > 2.5
                    state = self.brain._state_key(hp, mp, target_hp, in_melee, enemy_count=enemy_count, stalled=is_stuck)

                    current_action = self.brain.choose_action(
                        state, mp, is_stuck=is_stuck, skill_last_used=self.skill_last_used, now=now
                    )
                    self.execute_action(current_action)

                    if mp > 25.0 and random.random() < 0.20:
                        self.motor.shoot_right()

                    time.sleep(0.11)
                    new_hp = self.vision.get_player_hp()
                    new_mp = self.vision.get_player_mp()
                    new_target = self.vision.get_target_hp()

                    dmg_dealt = (target_hp - new_target) if (target_hp is not None and new_target is not None) else target_hp
                    dmg_val = max(0.0, float(dmg_dealt or 0.0))
                    self.brain.record_skill_outcome(current_action, dmg_val)
                    if dmg_val <= 0.2 and current_action in OFFENSIVE_SKILLS:
                        self.no_dmg_streak += 1
                    elif dmg_val > 0.2:
                        self.no_dmg_streak = 0

                    is_kill = new_target is None
                    reward = self.calculate_reward(
                        hp, new_hp, mp, new_mp, target_hp, new_target,
                        current_action, is_stuck=is_stuck, duration_sec=(time.time() - self.combat_start),
                    )
                    if not is_kill:
                        new_in_melee = (time.time() - self.combat_start) > 2.2 or (hp - new_hp) > 2.5
                        next_state = self.brain._state_key(new_hp, new_mp, new_target, new_in_melee, enemy_count=enemy_count)
                        self.brain.learn(state, current_action, reward, next_state, is_terminal=False)

                    self.last_hp = new_hp
                    self.last_mp = new_mp
                    self.last_target_hp = new_target
            except KeyboardInterrupt:
                break
            except Exception as e:
                AionLogger.log(f"[LOOP RECOVERED] {e}")
                time.sleep(0.2)

        self.brain.save_memory()
        self._write_status_telemetry(self.last_hp, self.last_mp, None, "STOPPED", 0.0, force=True)
        self.speech.speak(f"Aion Nexus finished. {self.kills} Kills reinforced!", force=True)
        AionLogger.log(f"=== AION NEXUS V5 SESSION FINISHED ({self.kills} Kills) ===")


def _run_self_test():
    """Offline end-to-end verification: NumPy vision, Q(lambda) arena, step_back & window guard."""
    import tempfile

    print("[AION NEXUS GAMER V5] Running Self-Verification Suite...")

    # 1. Vectorized NumPy Vision Tests
    hp_buf = np.zeros((4, 100, 4), dtype=np.uint8)
    hp_buf[:, :72, 2] = 210  # 72% red in BGRX
    assert abs(AionVision.parse_hp_bgra(hp_buf) - 72.0) < 0.2

    mp_buf = np.zeros((3, 100, 4), dtype=np.uint8)
    mp_buf[:, :55, 0] = 220  # 55% blue in BGRX
    assert abs(AionVision.parse_mp_bgra(mp_buf) - 55.0) < 0.2

    tgt_buf = np.zeros((9, 100, 4), dtype=np.uint8)
    tgt_buf[4, :40, 2] = 220
    assert abs(AionVision.parse_target_bgra(tgt_buf) - 40.0) < 0.2
    assert AionVision.parse_target_bgra(np.zeros((9, 100, 4), dtype=np.uint8)) is None

    mm_buf = np.zeros((20, 30, 4), dtype=np.uint8)
    mm_buf[5:8, 2:5, 2] = 220  # Hostile red blip on left minimap sector
    radar = AionVision.parse_minimap_radar_bgra(mm_buf)
    assert radar["total"] > 0 and radar["steer_dir"] == -1
    print(" -> Vision BGRA & Minimap Radar steering passed.")

    # 2. Motor Safety Guard & Fixed step_back() Verification
    geom = DynamicWindowGeometry(win_titles=("__NON_EXISTENT_TEST_WINDOW__",))
    motor = AionMotor(geom, disp=None, dry_run=True)
    assert not motor._can_send_input()
    motor.step_back(duration=0.01)
    assert motor.actions_dispatched >= 2
    print(" -> Window safety guard & AionMotor.step_back() passed.")

    # 3. Simulated Arena Q(lambda) + Action Masking + Cooldown Adaptation Test
    tmp_mem = os.path.join(tempfile.gettempdir(), "aion_gamer_v5_test_mem.json")
    if os.path.exists(tmp_mem):
        os.remove(tmp_mem)
    brain = NexusCombatBrain(memory_file=tmp_mem)

    skill_last = {k: 0.0 for k in SKILL_KEY_MAP}
    skill_last["SKILL_R"] = 1000.0  # On cooldown at now=1001.0
    avail = brain.available_actions(mp=80.0, skill_last_used=skill_last, now=1001.0)
    assert "SKILL_R" not in avail and "L_AUTO_SHOT" in avail
    avail_low_mp = brain.available_actions(mp=10.0, skill_last_used=skill_last, now=1001.0)
    assert "R_HEAVY_SHOT" not in avail_low_mp

    # Simulate 25 arena kills to prove Skill Q(lambda) prefers high-DPS burst over passive jump
    true_dmg = {
        "SKILL_R": 38.0, "SKILL_4": 26.0, "SKILL_1": 18.0, "SKILL_2": 15.0,
        "SKILL_Q": 20.0, "R_HEAVY_SHOT": 14.0, "L_AUTO_SHOT": 8.0,
        "TACTICAL_KITE": 5.0, "STRAFE_LEFT": 5.0, "STRAFE_RIGHT": 5.0, "JUMP_UNSTUCK": 2.0,
    }
    sim_time = 2000.0
    last_used = {k: 0.0 for k in SKILL_KEY_MAP}
    total_sim_kills = 0
    for ep in range(25):
        mob_hp = 100.0
        pl_hp = 100.0
        step = 0
        while mob_hp > 0 and step < 25:
            step += 1
            sim_time += 0.4
            st = brain._state_key(pl_hp, 90.0, mob_hp, in_melee=(step > 3))
            act = brain.choose_action(st, 90.0, skill_last_used=last_used, now=sim_time)
            if act in SKILL_KEY_MAP:
                last_used[act] = sim_time
            dmg = true_dmg.get(act, 10.0)
            mob_next = max(0.0, mob_hp - dmg)
            brain.record_skill_outcome(act, min(mob_hp, dmg))
            is_kill = mob_next <= 0.0
            rew = dmg * 0.45 + (28.0 if is_kill else 0.0)
            ns = brain._state_key(pl_hp, 90.0, None if is_kill else mob_next, in_melee=(step > 3))
            brain.learn(st, act, rew, ns, is_terminal=is_kill)
            mob_hp = mob_next
            if is_kill:
                total_sim_kills += 1
                brain.replay_step(batch_size=4)
                brain.decay_exploration()

    brain.save_memory()
    reloaded = NexusCombatBrain(memory_file=tmp_mem)
    assert os.path.exists(tmp_mem)
    top_full_act = max(
        ("SKILL_1", "SKILL_4", "SKILL_R", "SKILL_Q", "JUMP_UNSTUCK"),
        key=lambda a: reloaded.q_table.get("full_high_high_range_single", {}).get(a, 0.0),
    )
    assert top_full_act != "JUMP_UNSTUCK", f"Expected burst skill over JUMP_UNSTUCK, got {reloaded.q_table.get('full_high_high_range_single')}"
    print(f" -> Arena RL converged ({total_sim_kills} kills, states={len(reloaded.q_table)}, top_opener={top_full_act}, SKILL_R avg_dmg={reloaded.skill_stats['SKILL_R']['avg_dmg']}).")
    os.remove(tmp_mem)
    print("[SUCCESS] All AION NEXUS V5 Gamer self-tests passed!")


if __name__ == "__main__":
    args = [a.lower().lstrip("-") for a in sys.argv[1:]]
    if "self-test" in args or "test" in args:
        _run_self_test()
        sys.exit(0)

    mode = "QUEST"
    if "farm" in args or "autofarm" in args:
        mode = "FARM"
    elif "quest" in args:
        mode = "QUEST"

    gamer = AionNexusAutonomousGamer(initial_mode=mode)
    gamer.run()
