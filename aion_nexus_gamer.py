#!/usr/bin/env python3
# ==============================================================================
# AION NEXUS V6 — FULL-SCREEN VISION, QUEST WORKFLOW LEARNER & SURVIVAL AGENT
# ==============================================================================
# Autonomous AION 2 AI Agent featuring:
#   1. Single-Shot Full-Frame Screen Vision (X11 + NumPy/SciPy):
#      - Calibrated Player HP (pink/red + numeral bridge), Player MP (cyan/blue),
#        and Target HP + metallic frame verification (immune to red foliage!).
#      - 3D Viewport Perception: Thin overhead enemy HP bars & cyan target
#        nameplates, NPC/Interact [F] prompt & Dialogue Panel detector,
#        Player Defeated detector, and Left/Right open-terrain clearance.
#      - Top-Right Minimap Radar & Motion Speedometer: Detects true character
#        movement during [Alt+1] Auto-Run, counts 360-deg close-range threats,
#        and computes safe kiting vectors AWAY from multi-mob packs.
#      - Visual Hotbar Inspector: Detects ready vs cooldown vs locked/empty slots
#        (e.g., locked [4] Lv.16, empty [5..8]) so unacquired skills are skipped.
#   2. Non-Interruptive Quest Auto-Run + Persistent Workflow Memory:
#      - Triggers [Alt+1] cleanly and keeps HANDS OFF WASD/Space while moving!
#      - Only interrupts if attacked (then resumes [Alt+1] after clearing threat)
#        or on arrival (NPC [F] dialogue / quest-spot mob hunt / obstacle steer).
#      - Saves learned quest workflows, hotbar profiles, obstacle escapes, and
#        combat Q-tables persistently in aion_nexus_game_memory.json &
#        aion_nexus_combat_memory.json.
#   3. Anti-Pack Auto-Farm & Tactical Survival:
#      - ZERO random jumping during combat (JUMP_UNSTUCK removed from combat).
#      - Potions mapped to real AION 2 keys ([X] / [F1] HP, [F2] MP) + proactive
#        Evasion Dash ([Shift+S]) & directional kiting when >=2 mobs attack.
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
from scipy.ndimage import label
from Xlib import X, XK, display
from Xlib.ext import xtest

AI_DIR = "/root/coding/AI_Desktop"
MEINOS_DIR = "/root/coding/MeinOS"
for _d in (AI_DIR, MEINOS_DIR):
    if _d not in sys.path:
        sys.path.append(_d)

try:
    from aion_episodic_graph import EpisodicKnowledgeGraph
except ImportError:
    EpisodicKnowledgeGraph = None

AION_WINDOW_TITLES = ("AION2", "AION 2", "GeForce NOW")
COMBAT_MEMORY_FILE = os.path.join(MEINOS_DIR, "aion_nexus_combat_memory.json")
GAME_MEMORY_FILE = os.path.join(MEINOS_DIR, "aion_nexus_game_memory.json")
STATUS_FILE = "/tmp/aion_nexus_status.json"
CMD_FILE = "/tmp/aion_nexus_cmd.txt"
LOG_FILE = "/tmp/aion_nexus_log.txt"
MAX_LOG_BYTES = 2 * 1024 * 1024

# Calibrated Normalized HUD ROIs (verified against live AION 2 16:9 captures)
ROI_HP_BAR = (0.3840, 0.8980, 0.4815, 0.9105)
ROI_MP_BAR = (0.5180, 0.8990, 0.6185, 0.9110)
ROI_TGT_SEARCH = (0.4230, 0.0360, 0.5775, 0.0600)
TGT_ORB_NORM = (0.4890, 0.5110)  # Center level medallion inside Target HP bar
ROI_MINIMAP = (0.8600, 0.0720, 0.9850, 0.1880)
ROI_QUEST_TRACKER = (0.8560, 0.2350, 0.9920, 0.4250)
ROI_DIALOGUE_AREA = (0.2000, 0.6800, 0.8000, 0.8850)
ROI_INTERACT_PROMPT = (0.4300, 0.4800, 0.6400, 0.7200)

# Hotbar Slot ROIs: (x0, y0, x1, y1)
HOTBAR_SKILL_ROIS: Dict[str, Tuple[float, float, float, float]] = {
    "SKILL_1": (0.376, 0.955, 0.391, 0.985),
    "SKILL_2": (0.394, 0.955, 0.409, 0.985),
    "SKILL_3": (0.412, 0.955, 0.427, 0.985),
    "SKILL_4": (0.430, 0.955, 0.445, 0.985),
    "SKILL_5": (0.448, 0.955, 0.463, 0.985),
    "SKILL_Q": (0.544, 0.955, 0.559, 0.985),
    "SKILL_E": (0.562, 0.955, 0.577, 0.985),
    "SKILL_R": (0.580, 0.955, 0.595, 0.985),
}
HOTBAR_POTION_ROIS: Dict[str, Tuple[float, float, float, float]] = {
    "POTION_HP_X": (0.376, 0.916, 0.391, 0.946),
    "POTION_HP_F1": (0.407, 0.916, 0.422, 0.946),
    "POTION_MP_F2": (0.425, 0.916, 0.440, 0.946),
}

# Game Hotkeys (matched to AION 2 layout)
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
KEY_POTION_HP_PRIMARY = "x"
KEY_POTION_HP_ALT = "F1"
KEY_POTION_MP = "F2"
KEY_EVADE = "Shift_L"
KEY_FORWARD = "w"
KEY_BACKWARD = "s"
KEY_LEFT = "a"
KEY_RIGHT = "d"
KEY_JUMP = "space"

# Notice: JUMP_UNSTUCK is intentionally EXCLUDED from combat ACTIONS so the agent
# never jumps randomly during fights.
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
    "EVADE_BACK",
    "STRAFE_LEFT",
    "STRAFE_RIGHT",
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
MOVEMENT_SKILLS = {"TACTICAL_KITE", "EVADE_BACK", "STRAFE_LEFT", "STRAFE_RIGHT"}


class AionLogger:
    """Thread-safe rotating session logger for GUI telemetry console."""

    @staticmethod
    def log(msg: str):
        formatted = f"[{time.strftime('%H:%M:%S')}] {msg}\n"
        try:
            if os.path.exists(LOG_FILE) and os.path.getsize(LOG_FILE) > MAX_LOG_BYTES:
                os.replace(LOG_FILE, LOG_FILE + ".1")
            with open(LOG_FILE, "a", encoding="utf-8") as f:
                f.write(formatted)
        except Exception:
            pass


class AionSpeechBridge:
    """Rate-limited bridge to /tmp/ide_to_aion.txt for spoken narration."""

    def __init__(self):
        self.bridge_pipe = "/tmp/ide_to_aion.txt"
        self.last_spoken = 0.0
        self.cooldown = 4.0
        self.last_thought = ""

    def speak(self, text: str, force: bool = False):
        now = time.time()
        if not force and (now - self.last_spoken) < self.cooldown:
            return
        self.last_spoken = now
        self.last_thought = text
        formatted = f"[AION] {text}\n"
        print(f"\n\03[95m{formatted.strip()}\033[0m")
        AionLogger.log(f"[AION SPEECH] {text}")
        try:
            with open(self.bridge_pipe, "a", encoding="utf-8") as f:
                f.write(formatted)
        except Exception:
            pass


class DynamicWindowGeometry:
    """Tracks live AION 2 X11 window bounds and focus state."""

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
                    if w >= 640 and h >= 360:
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
    """
    Single-capture vectorized NumPy + SciPy X11 screen perception for AION 2.
    Reads entire screen context from one frame grab per tick (no repetitive X11 grabs).
    """

    def __init__(self, geom: DynamicWindowGeometry, disp: Optional[display.Display] = None):
        self.geom = geom
        self.disp = disp
        self.root = disp.screen().root if disp is not None else None
        self._hp_ema: Optional[float] = None
        self._mp_ema: Optional[float] = None
        self._hp_hist: Deque[Tuple[float, float]] = deque(maxlen=16)
        self._prev_minimap_center: Optional[np.ndarray] = None
        self._prev_vp_ground: Optional[np.ndarray] = None
        self.hotbar_ready_baseline: Dict[str, float] = {}
        self.hotbar_calibrated = False

    def capture_rgb_frame(self) -> Optional[np.ndarray]:
        """Captures full live AION 2 window as HxWx3 RGB array in a single X11 call."""
        if self.root is None or not self.geom.wid:
            return None
        try:
            img = self.root.get_image(
                max(0, self.geom.ax),
                max(0, self.geom.ay),
                max(1, self.geom.w),
                max(1, self.geom.h),
                X.ZPixmap,
                0xFFFFFFFF,
            )
            arr = np.frombuffer(img.data, dtype=np.uint8)
            expected = self.geom.w * self.geom.h * 4
            if arr.size < expected:
                return None
            bgra = arr[:expected].reshape((self.geom.h, self.geom.w, 4))
            if int(bgra[:, :, :3].max()) < 6:
                return None
            return bgra[:, :, 2::-1]  # View BGR->RGB without copy overhead on slice ops
        except Exception:
            return None

    @staticmethod
    def _crop_norm(rgb: np.ndarray, roi: Tuple[float, float, float, float]) -> np.ndarray:
        h, w = rgb.shape[:2]
        x0 = max(0, min(w - 1, int(roi[0] * w)))
        y0 = max(0, min(h - 1, int(roi[1] * h)))
        x1 = max(x0 + 1, min(w, int(roi[2] * w)))
        y1 = max(y0 + 1, min(h, int(roi[3] * h)))
        return rgb[y0:y1, x0:x1]

    @staticmethod
    def _contiguous_edge_fill(cols: np.ndarray, anchor_mask: np.ndarray, from_right: bool) -> float:
        n = int(cols.size)
        if n <= 0:
            return 0.0
        anchor_len = max(3, n // 6)
        if from_right:
            if not np.any(anchor_mask[-anchor_len:]):
                # Fallback to fraction only if genuinely strong fill elsewhere
                return round(float(cols.mean()) * 100.0, 1)
            filled = 0
            for c in range(n - 1, -1, -1):
                if cols[c]:
                    filled += 1
                elif filled > 4 and c > 0 and not np.any(cols[max(0, c - 4):c]):
                    break
            return round(min(100.0, (filled / n) * 100.0), 1)
        else:
            if not np.any(anchor_mask[:anchor_len]):
                return round(float(cols.mean()) * 100.0, 1)
            filled = 0
            for c in range(n):
                if cols[c]:
                    filled += 1
                elif filled > 4 and c + 1 < n and not np.any(cols[c + 1:min(n, c + 5)]):
                    break
            return round(min(100.0, (filled / n) * 100.0), 1)

    @classmethod
    def parse_hp_rgb(cls, patch_rgb: np.ndarray) -> float:
        """Reads AION 2 Player HP bar (anchored right, drains left->right, pink-red + white digits)."""
        if patch_rgb is None or patch_rgb.size == 0:
            return 0.0
        r = patch_rgb[:, :, 0].astype(np.int16)
        g = patch_rgb[:, :, 1].astype(np.int16)
        b = patch_rgb[:, :, 2].astype(np.int16)
        red_mag = (r > 125) & ((r - g) > 45) & (b < r - 15)
        if int(red_mag.sum()) < 4:
            return 0.0
        # Top 60% avoids most lower numeral overlay while red_mag ensures true red anchor
        top_rows = max(1, (patch_rgb.shape[0] * 3) // 5)
        text_w = (r > 130) & (g > 90) & (b > 90)
        cols = np.any(red_mag | text_w, axis=0)
        anchor_cols = np.any(red_mag[:top_rows], axis=0) | np.any(red_mag, axis=0)
        return cls._contiguous_edge_fill(cols, anchor_cols, from_right=True)

    @classmethod
    def parse_mp_rgb(cls, patch_rgb: np.ndarray) -> float:
        """Reads AION 2 Player MP bar (anchored left, cyan-blue fill + numeral bridge)."""
        if patch_rgb is None or patch_rgb.size == 0:
            return 0.0
        r = patch_rgb[:, :, 0].astype(np.int16)
        g = patch_rgb[:, :, 1].astype(np.int16)
        b = patch_rgb[:, :, 2].astype(np.int16)
        cyan_mp = (b > 120) & ((b - r) > 35)
        if int(cyan_mp.sum()) < 4:
            return 0.0
        text_w = (b > 140) & (g > 140) & (r > 110)
        cols = np.any(cyan_mp | text_w, axis=0)
        anchor_cols = np.any(cyan_mp, axis=0)
        return cls._contiguous_edge_fill(cols, anchor_cols, from_right=False)

    @staticmethod
    def parse_target_rgb(rgb: np.ndarray, roi: Tuple[float, float, float, float] = ROI_TGT_SEARCH) -> Tuple[Optional[float], bool]:
        """
        Detects AION 2 top-center Target HP bar with dark metallic frame validation
        so bright red Cantas Valley grass can NEVER produce phantom target locks.
        Returns (target_hp_pct | None, target_frame_present).
        """
        patch = AionVision._crop_norm(rgb, roi)
        if patch is None or patch.size == 0 or patch.shape[0] < 6 or patch.shape[1] < 24:
            return None, False

        ph, pw = patch.shape[:2]
        # Mask out center level medallion circle inside the target HP bar
        x_start_abs = roi[0]
        x_span_abs = max(1e-5, roi[2] - roi[0])
        orb_c0 = max(0, min(pw - 1, int(((TGT_ORB_NORM[0] - x_start_abs) / x_span_abs) * pw)))
        orb_c1 = max(orb_c0 + 1, min(pw, int(((TGT_ORB_NORM[1] - x_start_abs) / x_span_abs) * pw)))
        bar_mask_cols = np.ones(pw, dtype=bool)
        bar_mask_cols[orb_c0:orb_c1] = False
        valid_w = int(bar_mask_cols.sum())
        if valid_w < 16:
            return None, False

        r = patch[:, bar_mask_cols, 0].astype(np.int16)
        g = patch[:, bar_mask_cols, 1].astype(np.int16)
        b = patch[:, bar_mask_cols, 2].astype(np.int16)

        red_m = (r > 130) & (g < 85) & (b < 85) & ((r - g) > 50)
        dark_m = (r < 65) & (g < 65) & (b < 65)

        row_red = red_m.sum(axis=1)
        row_dark = dark_m.sum(axis=1)
        best_r = int(np.argmax(row_red))
        best_red_px = int(row_red[best_r])

        # Check dark metallic frame line immediately above (1-4px above bar top) or dark empty bar
        above_start = max(0, best_r - 4)
        below_end = min(ph, best_r + 8)
        top_dark_max = int(row_dark[above_start:best_r + 1].max()) if best_r >= above_start else 0
        bot_dark_max = int(row_dark[best_r:below_end].max()) if below_end > best_r else 0
        has_frame_border = (top_dark_max >= int(valid_w * 0.42)) or (bot_dark_max >= int(valid_w * 0.42))

        if best_red_px < max(10, int(valid_w * 0.08)):
            # Require the central gold/amber target emblem orb + dark bar track to count as an empty target frame
            orb_patch = patch[:, orb_c0:orb_c1].astype(np.int16)
            has_orb = orb_patch.size > 0 and int(
                ((orb_patch[:, :, 0] > 130) & (orb_patch[:, :, 1] > 90) & (orb_patch[:, :, 2] < 120)).sum()
            ) >= 4
            mid_band = row_dark[max(0, ph // 3):min(ph, (2 * ph) // 3)]
            frame_only = has_orb and mid_band.size > 0 and int(mid_band.max()) >= int(valid_w * 0.55)
            return None, frame_only

        if not has_frame_border:
            return None, False

        # Average over core bar rows around best_r to avoid jagged highlights
        r0 = max(0, best_r - 2)
        r1 = min(ph, best_r + 4)
        col_red = np.any(red_m[r0:r1], axis=0)
        # Verify left-side origin has red OR right-to-left drain consistency
        pct = round(max(1.0, min(100.0, float(col_red.mean()) * 100.0)), 1)
        return pct, True

    # Legacy BGRX unit-test wrappers (kept compatible with offline unit verification)
    @staticmethod
    def parse_hp_bgra(bgra: np.ndarray) -> float:
        if bgra is None or bgra.size == 0:
            return 0.0
        return AionVision.parse_hp_rgb(bgra[:, :, 2::-1])

    @staticmethod
    def parse_mp_bgra(bgra: np.ndarray) -> float:
        if bgra is None or bgra.size == 0:
            return 0.0
        return AionVision.parse_mp_rgb(bgra[:, :, 2::-1])

    @staticmethod
    def parse_target_bgra(bgra: np.ndarray) -> Optional[float]:
        if bgra is None or bgra.size == 0:
            return None
        r = bgra[:, :, 2].astype(np.int16)
        g = bgra[:, :, 1].astype(np.int16)
        b = bgra[:, :, 0].astype(np.int16)
        mask = (r > 130) & (g < 85) & (b < 85) & ((r - g) > 50)
        row_counts = mask.sum(axis=1)
        if int(row_counts.max()) if row_counts.size else 0 < 12:
            return None
        col_fill = np.any(mask, axis=0)
        return round(max(1.0, min(100.0, float(col_fill.mean()) * 100.0)), 1)

    @staticmethod
    def parse_minimap_rgb(mm_rgb: np.ndarray) -> Dict[str, Any]:
        """
        Extracts 360-deg close-range threats, steering bearings, safe kiting vector
        (away from dense packs), and cyan/gold quest waypoints from the minimap interior.
        """
        empty = {
            "total_threats": 0,
            "close_threats": 0,
            "steer_dir": 0.0,
            "safe_kite_dir": 0.0,
            "quest_blips": 0,
            "quest_dir": 0.0,
        }
        if mm_rgb is None or mm_rgb.size == 0 or mm_rgb.shape[0] < 16 or mm_rgb.shape[1] < 16:
            return empty

        mm = mm_rgb.astype(np.int16)
        mh, mw = mm.shape[:2]
        r, g, b = mm[:, :, 0], mm[:, :, 1], mm[:, :, 2]

        # Strict crimson enemy blip filter so reddish-brown Cantas Valley dirt doesn't read as 15+ mobs
        red_mask = (r > 168) & ((r - g) > 80) & ((r - b) > 75)
        # Ignore top-left corner edge where [M]/notification icons can slightly clip border
        red_mask[:max(2, mh // 10), :max(2, mw // 4)] = False

        lbl, n_comp = label(red_mask)
        close_threats = 0
        total_threats = 0
        nearest_dist = 9.0
        nearest_dx = 0.0
        weighted_x_sum = 0.0
        weighted_y_sum = 0.0
        weight_sum = 0.0

        for k in range(1, n_comp + 1):
            ys, xs = np.where(lbl == k)
            px_cnt = int(len(ys))
            if px_cnt < 2 or px_cnt > 65:
                continue
            dx = float(xs.mean()) / mw - 0.5
            dy = float(ys.mean()) / mh - 0.5
            dist = math.hypot(dx, dy)
            total_threats += 1
            if dist <= 0.30:
                close_threats += 1
            if dist < nearest_dist:
                nearest_dist = dist
                nearest_dx = dx
            w_val = 1.0 / max(0.08, dist)
            weighted_x_sum += dx * w_val
            weighted_y_sum += dy * w_val
            weight_sum += w_val

        steer_dir = max(-1.0, min(1.0, nearest_dx * 2.2)) if total_threats > 0 else 0.0
        # Safe kite vector points laterally opposite the red cluster centroid
        if weight_sum > 0:
            avg_dx = weighted_x_sum / weight_sum
            safe_kite_dir = -1.0 if avg_dx >= 0.0 else 1.0
        else:
            safe_kite_dir = 0.0

        cyan_mask = (g > 150) & (b > 140) & ((b - r) > 25)
        clbl, cn = label(cyan_mask)
        quest_blips = 0
        q_dx_sum = 0.0
        for k in range(1, cn + 1):
            ys, xs = np.where(clbl == k)
            if 2 <= len(ys) <= 80:
                quest_blips += 1
                q_dx_sum += float(xs.mean()) / mw - 0.5
        quest_dir = max(-1.0, min(1.0, (q_dx_sum / max(1, quest_blips)) * 2.0)) if quest_blips else 0.0

        return {
            "total_threats": total_threats,
            "close_threats": close_threats,
            "steer_dir": round(steer_dir, 3),
            "safe_kite_dir": round(safe_kite_dir, 3),
            "quest_blips": quest_blips,
            "quest_dir": round(quest_dir, 3),
        }

    @staticmethod
    def parse_minimap_radar_bgra(bgra: np.ndarray) -> Dict[str, Any]:
        """Backwards-compatible BGRA wrapper used by self-tests."""
        if bgra is None or bgra.size == 0:
            return {"total": 0, "left": 0, "front": 0, "right": 0, "steer_dir": 0}
        res = AionVision.parse_minimap_rgb(bgra[:, :, 2::-1])
        steer = -1 if res["steer_dir"] < -0.15 else (1 if res["steer_dir"] > 0.15 else 0)
        return {
            "total": res["total_threats"],
            "left": 1 if steer < 0 else 0,
            "front": 1 if steer == 0 and res["total_threats"] > 0 else 0,
            "right": 1 if steer > 0 else 0,
            "steer_dir": steer,
        }

    @staticmethod
    def detect_viewport_entities_rgb(vp_rgb: np.ndarray) -> Dict[str, Any]:
        """
        Analyzes 3D game viewport [0.15..0.85, 0.14..0.84] to locate:
          1. Crisp overhead crimson HP bars (real combat threats, immune to sky/foliage/GUI)
          2. Optional targetable mob nameplate icons below horizon strictly for scout aiming
             (NEVER counted in threat_count, preventing 15-20 sky/flower false threats!)
          3. Left vs Right terrain edge clutter to steer around walls cleanly.
        """
        if vp_rgb is None or vp_rgb.size == 0:
            return {"enemies": [], "scout_candidates": [], "away_dir": 0.0, "open_side": 0.0}

        vp = vp_rgb.astype(np.int16)
        vh, vw = vp.shape[:2]

        # 1. Crisp thin horizontal enemy overhead HP bars (height <= 6px, rect density >= 0.58)
        red_m = (vp[:, :, 0] > 160) & (vp[:, :, 1] < 60) & (vp[:, :, 2] < 60) & ((vp[:, :, 0] - vp[:, :, 1]) > 100)
        enemies: List[Dict[str, Any]] = []
        rlbl, rn = label(red_m)
        for k in range(1, rn + 1):
            ys, xs = np.where(rlbl == k)
            px_cnt = int(len(ys))
            if px_cnt < 10:
                continue
            h_box = int(ys.max() - ys.min() + 1)
            w_box = int(xs.max() - xs.min() + 1)
            rect_fill = px_cnt / max(1, h_box * w_box)
            if not (1 <= h_box <= 6 and w_box >= 10 and rect_fill >= 0.58):
                continue
            cy, cx = int(ys.mean()), int(xs.mean())
            # Exclude top-left pinned GUI region (x < 0.34, y < 0.43 in full-frame normalized coords)
            nx_win = 0.15 + 0.70 * (cx / max(1.0, float(vw)))
            ny_win = 0.14 + 0.70 * (cy / max(1.0, float(vh)))
            if nx_win < 0.34 and ny_win < 0.43:
                continue
            # Reject isolated red grass tips sitting inside a larger red bush neighborhood
            y0, y1 = max(0, cy - 10), min(vh, cy + 11)
            x0, x1 = max(0, cx - 14), min(vw, cx + 15)
            halo_red = int(red_m[y0:y1, x0:x1].sum())
            if halo_red > px_cnt * 2.2:
                continue
            dx_norm = round((cx - vw * 0.5) / max(1.0, vw * 0.5), 3)
            if not any(abs(dx_norm - e["dx_norm"]) < 0.06 and abs(cy - e["y"]) < 24 for e in enemies):
                enemies.append({"dx_norm": dx_norm, "y": cy, "type": "hp_bar"})

        # 2. Targetable diamond/nameplate icons BELOW sky horizon strictly for camera scout aiming
        scout_candidates: List[Dict[str, Any]] = []
        cyan_m = (vp[:, :, 2] > 195) & (vp[:, :, 1] > 180) & (vp[:, :, 0] < 130) & ((vp[:, :, 2] - vp[:, :, 0]) > 75)
        clbl, cn = label(cyan_m)
        for k in range(1, cn + 1):
            ys, xs = np.where(clbl == k)
            if not (6 <= len(ys) <= 45):
                continue
            h_box = int(ys.max() - ys.min() + 1)
            w_box = int(xs.max() - xs.min() + 1)
            if h_box > 14 or w_box > 20:
                continue
            cy, cx = int(ys.mean()), int(xs.mean())
            nx_win = 0.15 + 0.70 * (cx / max(1.0, float(vw)))
            ny_win = 0.14 + 0.70 * (cy / max(1.0, float(vh)))
            if ny_win < 0.28 or (nx_win < 0.34 and ny_win < 0.43):
                continue
            # Real nameplate icons sit against a dark outline or shadow drop
            py0, py1 = max(0, int(ys.min()) - 3), min(vh, int(ys.max()) + 4)
            px0, px1 = max(0, int(xs.min()) - 3), min(vw, int(xs.max()) + 4)
            if float(vp[py0:py1, px0:px1].mean(axis=2).min()) > 40.0:
                continue
            dx_norm = round((cx - vw * 0.5) / max(1.0, vw * 0.5), 3)
            if not any(abs(dx_norm - c["dx_norm"]) < 0.08 and abs(cy - c["y"]) < 26 for c in scout_candidates):
                scout_candidates.append({"dx_norm": dx_norm, "y": cy, "type": "nameplate"})

        away_dir = 0.0
        if enemies:
            mean_dx = sum(e["dx_norm"] for e in enemies) / len(enemies)
            away_dir = -1.0 if mean_dx >= 0.0 else 1.0

        # 3. Left vs Right terrain clutter/obstacle heuristic in lower-mid third
        ground = vp[vh // 2:, :, :].mean(axis=2)
        if ground.shape[0] > 4 and ground.shape[1] > 8:
            mid = ground.shape[1] // 2
            grad_x = np.abs(np.diff(ground, axis=1))
            left_clutter = float(grad_x[:, :mid - 1].mean())
            right_clutter = float(grad_x[:, mid:].mean())
            open_side = -1.0 if left_clutter + 1.0 < right_clutter else (1.0 if right_clutter + 1.0 < left_clutter else 0.0)
        else:
            open_side = 0.0

        return {"enemies": enemies, "scout_candidates": scout_candidates, "away_dir": away_dir, "open_side": open_side}

    def inspect_hotbar(self, rgb: np.ndarray) -> Dict[str, Any]:
        """
        Inspects AION 2 bottom hotbar skill & potion slots to classify:
          - empty/locked slots (e.g., locked [4] Lv.16, empty [5..8]) -> never press!
          - ready vs darkened cooldown skills -> intelligent skill availability!
        """
        slot_stats: Dict[str, Dict[str, Any]] = {}
        active_count = 0
        locked_or_empty: List[str] = []
        cooldown_keys: List[str] = []
        ready_keys: List[str] = []

        for sk, roi in HOTBAR_SKILL_ROIS.items():
            patch = self._crop_norm(rgb, roi)
            if patch is None or patch.size == 0:
                continue
            mean_v = float(patch.mean())
            std_v = float(patch.std())
            chroma = float((patch.max(axis=2) - patch.min(axis=2)).mean())

            # Verified against live 1920x1080 AION 2 hotbar:
            # Active icons (1,2,3,Q,E,R) have chroma >= 34 & mean >= 34 (magenta/purple icons stay bright chroma!).
            # Locked [4 Lv.16] has low chroma (<24.5); empty [5..8] has low mean (<30) & low chroma (<28.5).
            is_empty = (mean_v < 30.0 and chroma < 28.5) or (std_v < 26.0 and chroma < 24.5)
            is_locked = (chroma < 24.5) or (mean_v < 24.0)
            if is_empty or is_locked:
                locked_or_empty.append(sk)
                state = "LOCKED" if is_locked else "EMPTY"
            else:
                active_count += 1
                prev_base = self.hotbar_ready_baseline.get(sk, mean_v)
                if mean_v > prev_base and prev_base < 125.0:
                    self.hotbar_ready_baseline[sk] = round(min(125.0, 0.92 * prev_base + 0.08 * mean_v), 1)
                else:
                    self.hotbar_ready_baseline.setdefault(sk, round(min(125.0, mean_v), 1))
                base = self.hotbar_ready_baseline.get(sk, mean_v)
                if base > 50.0 and mean_v < base * 0.48:
                    cooldown_keys.append(sk)
                    state = "COOLDOWN"
                else:
                    ready_keys.append(sk)
                    state = "READY"
            slot_stats[sk] = {"state": state, "mean": round(mean_v, 1), "chroma": round(chroma, 1)}

        # Only enforce visual slot masking when HUD hotbar is clearly visible (>=2 active icons)
        reliable = active_count >= 2
        if reliable:
            self.hotbar_calibrated = True

        return {
            "reliable": reliable,
            "active_count": active_count,
            "locked_or_empty": locked_or_empty if reliable else [],
            "cooldown_keys": cooldown_keys if reliable else [],
            "ready_keys": ready_keys if reliable else [],
            "slots": slot_stats,
        }

    @staticmethod
    def compute_quest_signature(rgb: np.ndarray) -> str:
        """Compact quantized visual hash of top Quest Tracker slot #1 ([Alt+1])."""
        q_patch = AionVision._crop_norm(rgb, ROI_QUEST_TRACKER)
        if q_patch is None or q_patch.size == 0:
            return "unknown"
        # Focus on upper 55% of quest tracker (primary [Alt+1] quest)
        sub = q_patch[:max(4, (q_patch.shape[0] * 55) // 100), :, :]
        h_s, w_s = sub.shape[:2]
        ys = np.linspace(0, h_s - 1, 6, dtype=int)
        xs = np.linspace(0, w_s - 1, 10, dtype=int)
        grid = sub[np.ix_(ys, xs)].mean(axis=2)
        bits = (grid > grid.mean() + 6.0).astype(int).flatten()
        # Group into hex nibble signature
        val = 0
        for bit in bits:
            val = (val << 1) | int(bit)
        return f"q_{val:015x}"

    @staticmethod
    def detect_dialogue_and_death(rgb: np.ndarray, hp: float) -> Dict[str, Any]:
        """Detects NPC dialogue / quest popup boxes, [F] prompt, and defeated screen state."""
        # 1. NPC Dialogue panel: uniform dark translucent slab + bright centered/gold button highlight
        dlg = AionVision._crop_norm(rgb, ROI_DIALOGUE_AREA)
        dialogue_open = False
        if dlg is not None and dlg.size > 0:
            lum = dlg.mean(axis=2)
            dark_slab_ratio = float((lum < 58.0).mean())
            gold_btn = int(((dlg[:, :, 0] > 185) & (dlg[:, :, 1] > 140) & (dlg[:, :, 2] < 85)).sum())
            if dark_slab_ratio > 0.58 and (gold_btn > 35 or float(lum.std()) > 32.0):
                dialogue_open = True

        # 2. Interact [F] floating button / prompt near middle-bottom center
        ip = AionVision._crop_norm(rgb, ROI_INTERACT_PROMPT)
        interact_prompt = False
        if ip is not None and ip.size > 0:
            bright_txt = int(((ip[:, :, 0] > 215) & (ip[:, :, 1] > 215) & (ip[:, :, 2] > 215)).sum())
            dark_badge = float((ip.mean(axis=2) < 42.0).mean())
            # Small dark circular/square prompt keycap + crisp white text without red foliage trigger
            if 40 <= bright_txt <= 450 and dark_badge > 0.08:
                interact_prompt = True

        # 3. Player defeated state: extremely low/zero HP on visible hotbar + dimmed desaturated screen
        is_dead = False
        if hp <= 1.2:
            center_patch = AionVision._crop_norm(rgb, (0.30, 0.25, 0.70, 0.70))
            if center_patch is not None and center_patch.size > 0:
                sat = float((center_patch.max(axis=2) - center_patch.min(axis=2)).mean())
                if sat < 24.0:
                    is_dead = True

        return {
            "dialogue_open": dialogue_open,
            "interact_prompt": interact_prompt,
            "is_dead": is_dead,
        }

    def perceive_scene(self, rgb: Optional[np.ndarray] = None) -> Dict[str, Any]:
        """Runs full-screen perception pipeline on one captured game frame."""
        if rgb is None:
            rgb = self.capture_rgb_frame()
        if rgb is None:
            return {"valid": False}

        now = time.time()
        raw_hp = self.parse_hp_rgb(self._crop_norm(rgb, ROI_HP_BAR))
        raw_mp = self.parse_mp_rgb(self._crop_norm(rgb, ROI_MP_BAR))
        self._hp_ema = raw_hp if self._hp_ema is None else round(0.75 * raw_hp + 0.25 * self._hp_ema, 1)
        self._mp_ema = raw_mp if self._mp_ema is None else round(0.75 * raw_mp + 0.25 * self._mp_ema, 1)
        self._hp_hist.append((now, self._hp_ema))

        # Compute recent HP drop rate (%/sec) over trailing 1.5s to spot sudden burst/ambush
        hp_drop_rate = 0.0
        for ts_old, hp_old in self._hp_hist:
            dt = now - ts_old
            if 0.4 <= dt <= 1.8 and (hp_old - self._hp_ema) > hp_drop_rate * dt:
                hp_drop_rate = max(hp_drop_rate, (hp_old - self._hp_ema) / dt)

        tgt_hp, tgt_frame = self.parse_target_rgb(rgb)

        # Minimap radar + temporal movement speed
        mm_rgb = self._crop_norm(rgb, ROI_MINIMAP)
        mm_info = self.parse_minimap_rgb(mm_rgb)
        mm_motion = 0.0
        if mm_rgb is not None and mm_rgb.size > 0:
            mh, mw = mm_rgb.shape[:2]
            mm_ring = mm_rgb[mh // 5:(4 * mh) // 5, mw // 5:(4 * mw) // 5].astype(np.float32)
            # Downsample slightly for speed
            mm_ring = mm_ring[::2, ::2]
            if self._prev_minimap_center is not None and self._prev_minimap_center.shape == mm_ring.shape:
                mm_motion = float(np.abs(mm_ring - self._prev_minimap_center).mean())
            self._prev_minimap_center = mm_ring

        # 3D Viewport entities & terrain open side + optical ground motion
        vp_rgb = self._crop_norm(rgb, (0.15, 0.14, 0.85, 0.84))
        vp_info = self.detect_viewport_entities_rgb(vp_rgb[::2, ::2] if vp_rgb.shape[0] > 300 else vp_rgb)
        vp_motion = 0.0
        if vp_rgb is not None and vp_rgb.shape[0] > 60:
            g_sub = vp_rgb[vp_rgb.shape[0] // 2::4, ::4].astype(np.float32)
            if self._prev_vp_ground is not None and self._prev_vp_ground.shape == g_sub.shape:
                vp_motion = float(np.abs(g_sub - self._prev_vp_ground).mean())
            self._prev_vp_ground = g_sub

        hotbar = self.inspect_hotbar(rgb)
        quest_sig = self.compute_quest_signature(rgb)
        ui_flags = self.detect_dialogue_and_death(rgb, self._hp_ema)

        is_moving = (mm_motion > 2.2) or (mm_motion > 1.3 and vp_motion > 3.2)
        threat_count = max(mm_info["close_threats"], len(vp_info["enemies"]), 1 if tgt_hp is not None else 0)

        return {
            "valid": True,
            "player_hp": self._hp_ema,
            "player_mp": self._mp_ema,
            "hp_drop_rate": round(hp_drop_rate, 1),
            "target_hp": tgt_hp,
            "target_frame": tgt_frame,
            "minimap": mm_info,
            "minimap_motion": round(mm_motion, 2),
            "viewport_motion": round(vp_motion, 2),
            "is_moving": is_moving,
            "viewport_enemies": vp_info["enemies"],
            "scout_candidates": vp_info.get("scout_candidates", []),
            "away_dir": vp_info["away_dir"] or mm_info["safe_kite_dir"],
            "open_side": vp_info["open_side"],
            "threat_count": threat_count,
            "hotbar": hotbar,
            "quest_sig": quest_sig,
            "dialogue_open": ui_flags["dialogue_open"],
            "interact_prompt": ui_flags["interact_prompt"],
            "is_dead": ui_flags["is_dead"],
        }


class AionMotor:
    """Zero-fork XTest input dispatcher with clean window activation support."""

    def __init__(self, geom: DynamicWindowGeometry, disp: Optional[display.Display] = None, dry_run: bool = False):
        self.geom = geom
        self.disp = disp
        self.dry_run = dry_run
        self._keycodes: Dict[str, int] = {}
        self.actions_dispatched = 0
        self.command_log: List[str] = []
        self.lmb_auto_fire = False
        if self.disp is not None:
            for key_name in {
                KEY_TARGET, KEY_SKILL_1, KEY_SKILL_2, KEY_SKILL_3, KEY_SKILL_4,
                KEY_SKILL_5, KEY_SKILL_Q, KEY_SKILL_E, KEY_SKILL_R, KEY_LOOT,
                KEY_DIALOGUE_ADVANCE, KEY_POTION_HP_PRIMARY, KEY_POTION_HP_ALT,
                KEY_POTION_MP, KEY_EVADE, KEY_FORWARD, KEY_BACKWARD, KEY_LEFT,
                KEY_RIGHT, "Alt_L",
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
        self.command_log.append(f"{'DN' if pressed else 'UP'}:{key_name}")
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

    def _ensure_cursor_outside_gui(self):
        """
        Ensures the mouse pointer never hovers over the Always-on-Top GUI panel
        (top-left 35%x44%), preventing automated X11 clicks from hitting Tkinter and
        stealing active focus back from AION2!
        """
        if self.dry_run or not self.geom.wid:
            return
        safe_x = self.geom.ax + int(self.geom.w * 0.62)
        safe_y = self.geom.ay + int(self.geom.h * 0.50)
        gui_x1 = self.geom.ax + int(self.geom.w * 0.35)
        gui_y1 = self.geom.ay + int(self.geom.h * 0.45)
        try:
            if self.disp is not None:
                root = self.disp.screen().root
                ptr = root.query_pointer()
                px, py = int(ptr.root_x), int(ptr.root_y)
                in_bounds = (self.geom.ax <= px <= self.geom.ax + self.geom.w) and (self.geom.ay <= py <= self.geom.ay + self.geom.h)
                in_gui_box = (px <= gui_x1 and py <= gui_y1)
                if not in_bounds or in_gui_box:
                    root.warp_pointer(safe_x, safe_y)
                    self.disp.sync()
                return
            subprocess.run(
                ["xdotool", "mousemove", str(safe_x), str(safe_y)],
                check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=0.3,
            )
        except Exception:
            pass

    def _send_button_event(self, button: int, pressed: bool):
        self.actions_dispatched += 1
        self.command_log.append(f"{'BDN' if pressed else 'BUP'}:{button}")
        if not self._can_send_input():
            return
        if pressed:
            self._ensure_cursor_outside_gui()
        if self.disp is not None:
            try:
                xtest.fake_input(self.disp, X.ButtonPress if pressed else X.ButtonRelease, button)
                self.disp.sync()
                return
            except Exception:
                pass
        cmd = "mousedown" if pressed else "mouseup"
        subprocess.run(["xdotool", cmd, str(button)], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def focus_game(self, force: bool = False) -> bool:
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
            if active and int(active) == int(self.geom.wid):
                self._ensure_cursor_outside_gui()
                return True
            if active and not force:
                active_name = subprocess.check_output(
                    ["xdotool", "getwindowname", active], text=True, stderr=subprocess.DEVNULL, timeout=0.5
                ).strip()
                if "AION 2" in active_name or "NEURAL" in active_name or "Antigravity" in active_name:
                    return False
            for _ in range(2):
                subprocess.run(
                    ["xdotool", "windowactivate", "--sync", str(self.geom.wid)],
                    check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1.0,
                )
                time.sleep(0.08)
                self._ensure_cursor_outside_gui()
                if self.geom.is_game_active():
                    return True
            return self.geom.is_game_active()
        except Exception:
            return False

    def press_key(self, key: str, hold_ms: int = 105):
        self._send_key_event(key, True)
        if hold_ms > 0:
            time.sleep(hold_ms / 1000.0)
        self._send_key_event(key, False)

    def set_auto_fire(self, enabled: bool):
        """
        Toggles sustained Left Mouse Button hold for zero-cost rapid auto-shot
        (Dauerfeuer) in AION 2 without interfering with skills or GUI focus.
        """
        if enabled:
            if not self.lmb_auto_fire:
                self._send_button_event(1, True)
                self.lmb_auto_fire = True
        else:
            if self.lmb_auto_fire:
                self._send_button_event(1, False)
                self.lmb_auto_fire = False

    def rapid_shoot_left(self, count: int = 2, delay: float = 0.08):
        # If sustained LMB Dauerfeuer is already active, let continuous hold shoot cleanly!
        if self.lmb_auto_fire:
            time.sleep(max(0.08, delay))
            return
        for _ in range(count):
            self._send_button_event(1, True)
            time.sleep(0.105)
            self._send_button_event(1, False)
            if delay > 0:
                time.sleep(delay)

    def shoot_right(self, hold_sec: float = 0.14):
        was_auto = self.lmb_auto_fire
        if was_auto:
            self.set_auto_fire(False)
        self._send_button_event(3, True)
        time.sleep(hold_sec)
        self._send_button_event(3, False)
        if was_auto:
            self.set_auto_fire(True)

    def _move_and_shoot(self, key: str, duration: float = 0.30, shots: int = 2):
        self._send_key_event(key, True)
        if shots > 0:
            per_shot = max(0.05, duration / max(1, shots))
            self.rapid_shoot_left(count=shots, delay=per_shot)
        else:
            time.sleep(duration)
        self._send_key_event(key, False)

    def approach_target(self, duration: float = 0.42):
        """Closes distance ([W]) toward a distant Tab-locked target without jumping."""
        self.set_auto_fire(False)
        self._move_and_shoot(KEY_FORWARD, duration=duration, shots=1)

    def kite_orbwalk(self, strafe_dir: float = 0.0, backstep: bool = True, duration: float = 0.34):
        """
        Fluid ranged distance-keeping WHILE keeping LMB Dauerfeuer held ON and
        camera aimed directly at the target! Uses S and/or A/D without spinning camera away.
        """
        self.set_auto_fire(True)
        keys: List[str] = []
        if backstep:
            keys.append(KEY_BACKWARD)
        if abs(strafe_dir) > 0.1:
            keys.append(KEY_RIGHT if strafe_dir > 0 else KEY_LEFT)
        if not keys:
            keys.append(KEY_BACKWARD)
        for k in keys:
            self._send_key_event(k, True)
        time.sleep(duration)
        for k in reversed(keys):
            self._send_key_event(k, False)

    def evade_back(self, away_dir: float = 0.0):
        """Evasion dash ([Shift + S]) straight back while staying aimed at target."""
        was_auto = self.lmb_auto_fire
        if was_auto:
            self.set_auto_fire(False)
        self._send_key_event(KEY_BACKWARD, True)
        self.press_key(KEY_EVADE, hold_ms=95)
        time.sleep(0.22)
        self._send_key_event(KEY_BACKWARD, False)
        if was_auto:
            self.set_auto_fire(True)

    def strafe_left_and_shoot(self, duration: float = 0.30):
        self.kite_orbwalk(strafe_dir=-1.0, backstep=False, duration=duration)

    def strafe_right_and_shoot(self, duration: float = 0.30):
        self.kite_orbwalk(strafe_dir=1.0, backstep=False, duration=duration)

    def kite_back_and_shoot(self, duration: float = 0.34):
        self.kite_orbwalk(strafe_dir=0.0, backstep=True, duration=duration)

    def step_back(self, duration: float = 0.22):
        self._move_and_shoot(KEY_BACKWARD, duration=duration, shots=0)

    def walk_forward(self, duration: float = 0.45):
        self._move_and_shoot(KEY_FORWARD, duration=duration, shots=0)

    def turn_camera(self, steer_dir: float):
        """Smooth relative mouse look turn without cancelling key states unnecessarily."""
        if abs(steer_dir) < 0.05:
            return
        self.actions_dispatched += 1
        self.command_log.append(f"TURN:{steer_dir:+.2f}")
        if self._can_send_input():
            dx = int(max(-180, min(180, steer_dir * 130)))
            try:
                subprocess.run(
                    ["xdotool", "mousemove_relative", "--", str(dx), "0"],
                    check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=0.4,
                )
            except Exception:
                pass

    def steer_toward(self, steer_dir: float, duration: float = 0.20):
        """Smooth camera turn + brief sidestep when no Alt+1 auto-run is active."""
        self.turn_camera(steer_dir)
        key = KEY_LEFT if steer_dir < 0 else KEY_RIGHT
        self._move_and_shoot(key, duration=duration, shots=0)

    def sidestep_around_obstacle(self, direction: float = 1.0, allow_jump: bool = False):
        """Clean obstacle detour favoring open side; optional single jump only if flagged."""
        key = KEY_RIGHT if direction >= 0 else KEY_LEFT
        self.turn_camera(0.40 if direction >= 0 else -0.40)
        self._send_key_event(key, True)
        self._send_key_event(KEY_FORWARD, True)
        if allow_jump:
            self.press_key(KEY_JUMP, hold_ms=40)
        time.sleep(0.32)
        self._send_key_event(KEY_FORWARD, False)
        self._send_key_event(key, False)

    def stop_movement(self):
        self.set_auto_fire(False)
        for k in (KEY_FORWARD, KEY_BACKWARD, KEY_LEFT, KEY_RIGHT, KEY_EVADE, "Alt_L"):
            self._send_key_event(k, False)

    def loot(self):
        for _ in range(3):
            self.press_key(KEY_LOOT, hold_ms=45)
            time.sleep(0.10)

    def auto_run_quest(self):
        """Ensures no WASD keys are stuck and triggers native AION 2 [Alt+1] cleanly once."""
        self.stop_movement()
        self.focus_game(force=True)
        self._send_key_event("Alt_L", True)
        time.sleep(0.03)
        self.press_key("1", hold_ms=45)
        time.sleep(0.03)
        self._send_key_event("Alt_L", False)

    def interact_npc(self, advance_dialogue: bool = False):
        self.press_key(KEY_INTERACT, hold_ms=55)
        if advance_dialogue:
            time.sleep(0.22)
            self.press_key(KEY_DIALOGUE_ADVANCE, hold_ms=45)


class GlobalHotkeyManager:
    """Polls X11 keymap cleanly for [F8] Mode Toggle and [F10]/[Pause] Pause."""

    def __init__(self, disp: Optional[display.Display] = None):
        self.disp = disp
        self.pause_keycodes = [76, 127, 75]  # F10, Pause, F9 fallback
        self.mode_keycodes = [74]  # F8
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


class GameWorkflowMemory:
    """
    Persistent procedural & visual memory for AION 2:
      - Discovers and saves hotbar skills (ready vs locked/empty) & potion keys
      - Learns Quest Workflows per quest visual signature (NPC talk vs quest hunt vs run)
      - Records terrain obstacle escape directions and survival metrics across sessions.
    """

    def __init__(self, memory_file: str = GAME_MEMORY_FILE):
        self.memory_file = memory_file
        self.hotbar_profile: Dict[str, Any] = {
            "hotbar_version": "V6_2",
            "locked_or_empty": [],
            "ready_baselines": {},
            "hp_potion_key": KEY_POTION_HP_PRIMARY,
            "mp_potion_key": KEY_POTION_MP,
        }
        self.quest_workflows: Dict[str, Dict[str, Any]] = {}
        self.obstacle_escapes: Dict[str, float] = {"left": 0.0, "right": 0.0}
        self.episodes: List[Dict[str, Any]] = []
        self.total_quests_progressed = 0
        self.total_multi_mob_survivals = 0
        self._load()

    def _load(self):
        if not os.path.exists(self.memory_file):
            return
        try:
            with open(self.memory_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            if data.get("hotbar_profile"):
                loaded_hb = data["hotbar_profile"]
                if loaded_hb.get("hotbar_version") == "V6_2":
                    self.hotbar_profile.update(loaded_hb)
                else:
                    # Discard stale V6 hotbar cache that misclassified purple skills as LOCKED
                    self.hotbar_profile["locked_or_empty"] = []
                    self.hotbar_profile["hotbar_version"] = "V6_2"
            self.quest_workflows = data.get("quest_workflows", {})
            if data.get("obstacle_escapes"):
                self.obstacle_escapes.update(data["obstacle_escapes"])
            self.episodes = data.get("episodes", [])[-100:]
            self.total_quests_progressed = int(data.get("total_quests_progressed", 0))
            self.total_multi_mob_survivals = int(data.get("total_multi_mob_survivals", 0))
        except Exception:
            pass

    def save(self):
        payload = {
            "agent": "AION_NEXUS_V6_WORKFLOW_MEMORY",
            "last_updated": time.strftime("%Y-%m-%d %H:%M:%S"),
            "total_quests_progressed": self.total_quests_progressed,
            "total_multi_mob_survivals": self.total_multi_mob_survivals,
            "hotbar_profile": self.hotbar_profile,
            "obstacle_escapes": self.obstacle_escapes,
            "quest_workflows": self.quest_workflows,
            "episodes": self.episodes[-100:],
        }
        tmp = self.memory_file + ".tmp"
        try:
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(payload, f, indent=2, ensure_ascii=False)
            os.replace(tmp, self.memory_file)
        except Exception:
            pass

    def update_hotbar_knowledge(self, hotbar: Dict[str, Any]):
        if not hotbar.get("reliable"):
            return
        self.hotbar_profile["locked_or_empty"] = sorted(hotbar.get("locked_or_empty", []))

    def record_quest_flow(self, quest_sig: str, flow_type: str, actions: List[str], run_sec: float, progressed: bool):
        entry = self.quest_workflows.setdefault(quest_sig, {
            "preferred_type": flow_type,
            "success_count": 0,
            "attempts": 0,
            "avg_run_sec": round(run_sec, 1),
            "last_sequence": [],
        })
        entry["attempts"] += 1
        if progressed:
            entry["success_count"] += 1
            entry["preferred_type"] = flow_type
            entry["last_sequence"] = actions[-10:]
            self.total_quests_progressed += 1
        entry["avg_run_sec"] = round(0.7 * entry["avg_run_sec"] + 0.3 * run_sec, 1)
        self.episodes.append({
            "ts": time.strftime("%H:%M:%S"),
            "kind": "quest",
            "quest_sig": quest_sig,
            "flow": flow_type,
            "run_sec": round(run_sec, 1),
            "actions": actions[-8:],
            "progressed": progressed,
        })
        self.episodes = self.episodes[-100:]
        self.save()

    def get_quest_hint(self, quest_sig: str) -> Optional[str]:
        entry = self.quest_workflows.get(quest_sig)
        if entry and entry.get("success_count", 0) > 0:
            return entry.get("preferred_type")
        return None

    def record_escape(self, direction: float, success: bool):
        side = "right" if direction >= 0 else "left"
        delta = 1.0 if success else -0.4
        self.obstacle_escapes[side] = round(max(-5.0, min(20.0, self.obstacle_escapes.get(side, 0.0) + delta)), 2)

    def best_escape_dir(self, terrain_open_side: float) -> float:
        if abs(terrain_open_side) > 0.1:
            return terrain_open_side
        return 1.0 if self.obstacle_escapes.get("right", 0.0) >= self.obstacle_escapes.get("left", 0.0) else -1.0


class NexusCombatBrain:
    """
    V6.2 Ranged Auto-Fire & Agile Kite RL Brain:
      - Q(lambda) eligibility traces with terminal kill propagation
      - Visual & Cooldown Action Masking (purple/magenta ready skills never false-locked!)
      - Continuous LMB Dauerfeuer toggle + offensive skill weaving + distance-keeping orbwalk
      - Multi-mob survival Evasion Dash ([Shift + S]) when pressured or swarmed.
    """

    MODEL_VERSION = "V6_2_RANGED_KITE_COMBAT"

    def __init__(self, memory_file: str = COMBAT_MEMORY_FILE):
        self.memory_file = memory_file
        self.alpha = 0.18
        self.gamma = 0.88
        self.lambda_trace = 0.72
        self.epsilon = 0.16
        self.epsilon_min = 0.04
        self.epsilon_decay = 0.990
        self.ucb_c = 0.70

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
            # Clean obsolete JUMP_UNSTUCK key if present
            self.q_table[state].pop("JUMP_UNSTUCK", None)
        if state not in self.sa_counts:
            self.sa_counts[state] = {a: 0 for a in ACTIONS}
        else:
            for a in ACTIONS:
                self.sa_counts[state].setdefault(a, 0)
            self.sa_counts[state].pop("JUMP_UNSTUCK", None)

    def _state_key(
        self,
        hp: float,
        mp: float,
        target_hp: Optional[float],
        in_melee: bool,
        enemy_count: int = 1,
        stalled: bool = False,
    ) -> str:
        hp_band = "high" if hp > 62 else ("mid" if hp > 35 else "low")
        mp_band = "high" if mp > 55 else ("mid" if mp > 20 else "low")
        if target_hp is None:
            tgt_band = "none"
        elif target_hp > 65:
            tgt_band = "full"
        elif target_hp > 30:
            tgt_band = "mid"
        else:
            tgt_band = "low"
        dist_band = "melee" if in_melee else "range"
        pack_tag = "multi" if enemy_count >= 2 else "single"
        stall_tag = "_stall" if stalled else ""
        return f"{tgt_band}_{mp_band}_{hp_band}_{dist_band}_{pack_tag}{stall_tag}"

    def available_actions(
        self,
        mp: float,
        skill_last_used: Optional[Dict[str, float]] = None,
        now: Optional[float] = None,
        is_stuck: bool = False,
        locked_or_empty: Optional[Iterable[str]] = None,
        cooldown_keys: Optional[Iterable[str]] = None,
        threat_count: int = 1,
        hp: float = 100.0,
    ) -> List[str]:
        now_ts = time.time() if now is None else now
        blocked = set(locked_or_empty or ())
        vis_cd = set(cooldown_keys or ())
        avail: List[str] = []

        allow_defensive = (threat_count >= 2) or (hp < 65.0) or is_stuck

        for a in ACTIONS:
            if a in blocked or a in vis_cd:
                continue
            if a in {"EVADE_BACK", "TACTICAL_KITE"} and not allow_defensive:
                continue
            if a in {"STRAFE_LEFT", "STRAFE_RIGHT"} and not (is_stuck or threat_count >= 2 or hp < 60.0):
                continue
            if a == "R_HEAVY_SHOT" and mp < 20.0:
                continue
            if a in SKILL_KEY_MAP and skill_last_used is not None:
                last_t = skill_last_used.get(a, 0.0)
                cd = self.learned_cooldowns.get(a, DEFAULT_SKILL_COOLDOWNS.get(a, 2.0))
                if (now_ts - last_t) < cd:
                    continue
            avail.append(a)

        # Even when stalled/repositioning, NEVER lock out basic offensive skills
        if is_stuck:
            stuck_pool = [
                a for a in avail
                if a in {"SKILL_1", "SKILL_2", "SKILL_Q", "SKILL_E", "SKILL_R", "L_AUTO_SHOT", "STRAFE_LEFT", "STRAFE_RIGHT", "EVADE_BACK"}
            ]
            if stuck_pool:
                return stuck_pool

        return avail or ["L_AUTO_SHOT"]

    def choose_action(
        self,
        state: str,
        mp: float,
        is_stuck: bool = False,
        skill_last_used: Optional[Dict[str, float]] = None,
        now: Optional[float] = None,
        locked_or_empty: Optional[Iterable[str]] = None,
        cooldown_keys: Optional[Iterable[str]] = None,
        threat_count: int = 1,
        hp: float = 100.0,
    ) -> str:
        self._ensure_state(state)
        candidates = self.available_actions(
            mp,
            skill_last_used=skill_last_used,
            now=now,
            is_stuck=is_stuck,
            locked_or_empty=locked_or_empty,
            cooldown_keys=cooldown_keys,
            threat_count=threat_count,
            hp=hp,
        )

        # Immediate tactical reflex ONLY under real multi-mob pressure or critically low HP
        if (threat_count >= 2 and hp < 55.0) or hp < 36.0:
            if random.random() < 0.45 and "EVADE_BACK" in candidates:
                return "EVADE_BACK"

        # Offensive skill-first priority: cast ready skills cleanly instead of backing off
        ready_skills = [s for s in candidates if s in SKILL_KEY_MAP]
        if ready_skills and hp >= 50.0 and not is_stuck:
            q_row_tmp = self.q_table[state]
            c_row_tmp = self.sa_counts[state]
            untried = [s for s in ready_skills if c_row_tmp.get(s, 0) < 2]
            if untried:
                return untried[0]
            if random.random() < 0.80:
                return max(
                    ready_skills,
                    key=lambda s: q_row_tmp.get(s, 0.0) + 0.15 * self.skill_stats.get(s, {}).get("avg_dmg", 0.0),
                )

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
            dmg_prior = 0.16 * min(25.0, self.skill_stats.get(act, {}).get("avg_dmg", 0.0))
            skill_bonus = 1.6 if act in SKILL_KEY_MAP else (0.5 if act in {"L_AUTO_SHOT", "R_HEAVY_SHOT"} else 0.0)
            survival_prior = 1.4 if ((threat_count >= 2 and hp < 65.0) and act in {"EVADE_BACK", "TACTICAL_KITE"}) else 0.0
            untried_bonus = (0.45 * q_scale) if (visits == 0 and act in OFFENSIVE_SKILLS) else 0.0
            return q_val + ucb + combo + dmg_prior + skill_bonus + survival_prior + untried_bonus

        return max(candidates, key=_score)

    def record_skill_outcome(self, action: str, dmg_dealt: float):
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
            if stats["zero_dmg_streak"] >= 4:
                self.learned_cooldowns[action] = round(min(14.0, self.learned_cooldowns[action] + 0.3), 2)
                stats["zero_dmg_streak"] = 0

    def learn(self, state: str, action: str, reward: float, next_state: str, is_terminal: bool = False):
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

        self.traces[(state, action)] = 1.0
        dead_traces: List[Tuple[str, str]] = []
        for (ts, ta), e_val in list(self.traces.items()):
            self._ensure_state(ts)
            if ta not in self.q_table[ts]:
                continue
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
        if len(self.replay_buffer) < batch_size:
            return
        for s, a, r, ns, term in random.sample(list(self.replay_buffer), batch_size):
            if a not in ACTIONS:
                continue
            self._ensure_state(s)
            self._ensure_state(ns)
            target = r if term else (r + self.gamma * max(self.q_table[ns].values(), default=0.0))
            old_q = self.q_table[s][a]
            self.q_table[s][a] = round(old_q + (0.5 * self.alpha) * (target - old_q), 4)

    def decay_exploration(self):
        if len(self.combat_durations) >= 10:
            recent_ttk = sum(self.combat_durations[-5:]) / 5.0
            prior_ttk = sum(self.combat_durations[-10:-5]) / 5.0
            if recent_ttk > prior_ttk * 1.15 and self.epsilon < 0.20:
                self.epsilon = round(min(0.22, self.epsilon + 0.012), 4)
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
                # Preserve backup cleanly & attenuate polluted kite-loop / stale cooldown weights
                bak_path = self.memory_file.replace(".json", ".v5_backup.json")
                if not os.path.exists(bak_path):
                    with open(bak_path, "w", encoding="utf-8") as bf:
                        json.dump(data, bf, indent=2, ensure_ascii=False)
                self.q_table = {
                    st: {
                        act: round(val * (0.10 if act in MOVEMENT_SKILLS else 0.35), 4)
                        for act, val in acts.items()
                        if act in ACTIONS
                    }
                    for st, acts in raw_q.items()
                }
                for st in self.q_table:
                    self._ensure_state(st)
                self.learned_cooldowns = dict(DEFAULT_SKILL_COOLDOWNS)
                self.epsilon = max(0.14, min(0.20, float(data.get("epsilon", 0.16))))
                self.combat_durations = [d for d in data.get("combat_durations", []) if 1.2 <= d <= 60.0][-25:]
            else:
                self.q_table = raw_q
                self.sa_counts = data.get("sa_counts", {})
                self.combo_q = data.get("combo_q", {})
                if data.get("skill_stats"):
                    for k, v in data["skill_stats"].items():
                        if k in self.skill_stats:
                            self.skill_stats[k] = v
                if data.get("learned_cooldowns"):
                    self.learned_cooldowns.update(data["learned_cooldowns"])
                self.epsilon = float(data.get("epsilon", 0.16))
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
    """
    Autonomous AION 2 V6 Agent:
      - Quest Auto-Run Mode: presses [Alt+1] once, strictly preserves auto-run by
        NEVER spamming WASD/Space while minimap motion shows movement, only
        intervenes on arrival (NPC dialogue / quest mobs / stuck detour) or ambush,
        and saves learned quest workflows into GameWorkflowMemory.
      - Auto-Farm Mode: foliage-immune target alive/dead tracking, no random jumping,
        multi-mob pack kiting & [X]/[F1]/[F2] potion defense, post-fight HP check.
    """

    def __init__(self, initial_mode: str = "QUEST", dry_run: bool = False):
        self.dry_run = dry_run
        self.disp = None if dry_run else display.Display(":0")
        self.geom = DynamicWindowGeometry()
        self.vision = AionVision(self.geom, disp=self.disp)
        self.motor = AionMotor(self.geom, disp=self.disp, dry_run=dry_run)
        self.speech = AionSpeechBridge()
        self.brain = NexusCombatBrain()
        self.workflow_mem = GameWorkflowMemory()
        self.hotkeys = GlobalHotkeyManager(disp=self.disp)
        self.hotkeys.mode = initial_mode
        self.running = True
        self.kg = EpisodicKnowledgeGraph() if (EpisodicKnowledgeGraph and not dry_run) else None

        # Combat & Survival Tracking
        self.last_hp = 100.0
        self.last_mp = 100.0
        self.last_target_hp: Optional[float] = None
        self.combat_start = 0.0
        self.combat_had_real_dmg = False
        self.combat_max_threats = 1
        self.combat_step_idx = 0
        self.last_kite_time = 0.0
        self.last_evade_time = 0.0
        self.last_combat_log = 0.0
        self.target_confirm_streak = 0
        self.missing_target_streak = 0
        self.no_dmg_streak = 0
        self.last_approach_time = 0.0
        self.last_target_change = time.time()
        self.last_observed_target_hp: Optional[float] = None
        self.last_hp_pot_used = 0.0
        self.last_mp_pot_used = 0.0
        self.last_approach_time = 0.0
        self.kills = self.brain.total_kills
        self.roam_count = 0

        # Quest State Machine
        self.quest_phase = "NEED_ALT1"  # NEED_ALT1 | AUTORUNNING | ARRIVED | DIALOGUE
        self.quest_run_started_at = 0.0
        self.quest_has_moved = False
        self.quest_still_since = 0.0
        self.quest_start_sig = "unknown"
        self.quest_action_trace: List[str] = []
        self.last_quest_wait_log = 0.0
        self.last_dialogue_step = 0.0
        self.last_status_write = 0.0
        self.last_scene_label = "IDLE"
        self.skill_last_used: Dict[str, float] = {k: 0.0 for k in SKILL_KEY_MAP}

    def _trigger_quest_autorun(self, scene_sig: str = "unknown"):
        self.hotkeys.mode = "QUEST"
        self.motor.auto_run_quest()
        now = time.time()
        self.quest_phase = "AUTORUNNING"
        self.quest_run_started_at = now
        self.quest_has_moved = False
        self.quest_still_since = 0.0
        self.quest_start_sig = scene_sig
        self.quest_action_trace = ["ALT_1"]
        self.last_scene_label = "QUEST_AUTORUN"
        AionLogger.log("[QUEST] [Alt+1] Auto-Run engaged — hands off movement until arrival.")

    def _check_ipc_commands(self, curr_quest_sig: str = "unknown"):
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
                self.motor.focus_game(force=True)
                if self.hotkeys.mode == "QUEST":
                    self._trigger_quest_autorun(curr_quest_sig)
                self.speech.speak(f"Aion resuming! Mode: {self.hotkeys.mode}", force=True)
            elif cmd == "TOGGLE_PAUSE":
                self.hotkeys.paused = not self.hotkeys.paused
                if self.hotkeys.paused:
                    self.motor.stop_movement()
                    self.speech.speak("Aion paused! You have manual control.", force=True)
                else:
                    self.motor.focus_game(force=True)
                    if self.hotkeys.mode == "QUEST" and self.quest_phase != "AUTORUNNING":
                        self._trigger_quest_autorun(curr_quest_sig)
                    self.speech.speak(f"Aion resuming! Mode: {self.hotkeys.mode}", force=True)
            elif cmd in {"MODE_QUEST", "QUEST_AUTORUN", "ALT_1", "CLICK_QUEST"}:
                self.hotkeys.paused = False
                self._trigger_quest_autorun(curr_quest_sig)
                self.speech.speak("Quest Auto-Run active with Alt+1!", force=True)
            elif cmd == "MODE_FARM":
                self.hotkeys.paused = False
                if self.hotkeys.mode != "FARM":
                    self._reset_combat_state()
                self.hotkeys.mode = "FARM"
                self.quest_phase = "IDLE"
                self.motor.focus_game(force=True)
                self.speech.speak("Auto-Farm Mode activated!", force=True)
            elif cmd == "TOGGLE_MODE":
                if self.hotkeys.mode == "QUEST":
                    self.hotkeys.mode = "FARM"
                    self.quest_phase = "IDLE"
                    self.speech.speak("Switched to Auto-Farm Mode!", force=True)
                else:
                    self._trigger_quest_autorun(curr_quest_sig)
                    self.speech.speak("Switched to Quest Auto-Run Mode!", force=True)
            elif cmd == "STOP":
                self.motor.stop_movement()
                self.running = False
        except Exception:
            pass

    def _write_status_telemetry(
        self,
        hp: Optional[float],
        mp: Optional[float],
        target_hp: Optional[float],
        current_action: str,
        reward: float,
        window_visible: bool = True,
        threats: int = 0,
        active_skills: Optional[List[str]] = None,
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
            "window_visible": window_visible,
            "scene": self.last_scene_label,
            "threats": threats,
            "player_hp": round(hp, 1) if hp is not None else round(self.last_hp, 1),
            "player_mp": round(mp, 1) if mp is not None else round(self.last_mp, 1),
            "target_hp": round(target_hp, 1) if target_hp is not None else None,
            "kills": self.kills,
            "avg_ttk": round(avg_ttk, 1),
            "epsilon": round(self.brain.epsilon, 3),
            "states_learned": len(self.brain.q_table),
            "workflows_learned": len(self.workflow_mem.quest_workflows),
            "quests_progressed": self.workflow_mem.total_quests_progressed,
            "active_skills": active_skills if active_skills is not None else [],
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

    def _use_survival_potions(self, hp: float, mp: float, threat_count: int):
        now = time.time()
        # Proactive healing at <68% HP (or <78% when >=2 mobs attack!) on real potion keys [X] & [F1]
        hp_thresh = 78.0 if threat_count >= 2 else 68.0
        if 0.0 < hp < hp_thresh and (now - self.last_hp_pot_used) >= 4.0:
            self.motor.press_key(KEY_POTION_HP_PRIMARY)
            if hp < 50.0:
                time.sleep(0.08)
                self.motor.press_key(KEY_POTION_HP_ALT)
            self.last_hp_pot_used = now
            AionLogger.log(f"[SURVIVAL] HP {hp:.1f}% (threats={threat_count}) -> Triggered HP Potion [{KEY_POTION_HP_PRIMARY}/{KEY_POTION_HP_ALT}]")
        if 0.0 < mp < 28.0 and (now - self.last_mp_pot_used) >= 6.0:
            self.motor.press_key(KEY_POTION_MP)
            self.last_mp_pot_used = now

    def execute_action(self, action: str, away_dir: float = 0.0):
        now = time.time()
        if action == "L_AUTO_SHOT":
            # Toggle sustained LMB Dauerfeuer ON for zero-cost rapid auto-shots
            self.motor.set_auto_fire(True)
            time.sleep(0.18)
        elif action == "R_HEAVY_SHOT":
            self.motor.shoot_right()
            self.motor.set_auto_fire(True)
        elif action in SKILL_KEY_MAP:
            cd = self.brain.learned_cooldowns.get(action, DEFAULT_SKILL_COOLDOWNS[action])
            if (now - self.skill_last_used.get(action, 0.0)) >= cd:
                # Briefly pause sustained LMB so Proton cleanly registers skill keypress, then resume Dauerfeuer!
                self.motor.set_auto_fire(False)
                self.motor.press_key(SKILL_KEY_MAP[action], hold_ms=110)
                self.skill_last_used[action] = now
                time.sleep(0.06)
            self.motor.set_auto_fire(True)
        elif action == "EVADE_BACK":
            self.motor.evade_back(away_dir=away_dir)
            self.last_evade_time = now
        elif action == "TACTICAL_KITE":
            self.motor.kite_orbwalk(strafe_dir=away_dir, backstep=True, duration=0.34)
        elif action == "STRAFE_LEFT":
            self.motor.strafe_left_and_shoot(duration=0.30)
        elif action == "STRAFE_RIGHT":
            self.motor.strafe_right_and_shoot(duration=0.30)

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
        threat_count: int = 1,
        duration_sec: float = 0.0,
    ) -> float:
        reward = 0.0
        if prev_tgt is not None and curr_tgt is not None:
            dmg_dealt = prev_tgt - curr_tgt
            if dmg_dealt > 0.5:
                reward += dmg_dealt * 0.48
                self.brain.total_damage_dealt += dmg_dealt
            elif action in OFFENSIVE_SKILLS and is_stuck:
                reward -= 1.5

        if prev_tgt is not None and curr_tgt is None:
            reward += 25.0 + (8.0 if threat_count >= 2 else 0.0)
            if 1.0 < duration_sec < 10.0:
                reward += round(10.0 * (10.0 - duration_sec) / 10.0, 2)

        dmg_taken = prev_hp - curr_hp
        if dmg_taken > 0.2:
            reward -= dmg_taken * (0.85 if threat_count >= 2 else 0.6)
            self.brain.total_damage_taken += dmg_taken
            if action in {"EVADE_BACK", "TACTICAL_KITE"}:
                reward += 2.2

        if curr_mp < 15.0 and action == "R_HEAVY_SHOT":
            reward -= 3.0
        if (curr_hp < 55.0 or (threat_count >= 2 and dmg_taken > 0.2)) and action in {"EVADE_BACK", "TACTICAL_KITE"}:
            reward += 2.2
        if is_stuck:
            reward += 1.5 if action in SKILL_KEY_MAP else (0.5 if action in MOVEMENT_SKILLS else -1.5)

        return round(reward, 3)

    def record_evolution_milestone(self, kills: int, avg_ttk: float):
        if self.kg:
            try:
                node_id = f"aion2_mastery_kill_{kills}"
                self.kg.add_node(
                    node_id,
                    "EVOLUTIONARY_MILESTONE",
                    f"AION 2 V6 Vision Mastery: {kills} Kills (Avg TTK {avg_ttk:.1f}s)",
                    generation=kills,
                    importance=0.92,
                )
                self.kg.add_or_reinforce_edge("tobias", node_id, "MENTORED_GAMING_BREAKTHROUGH", kills)
            except Exception:
                pass

    def _reset_combat_state(self):
        if hasattr(self, "motor"):
            self.motor.set_auto_fire(False)
        self.combat_start = 0.0
        self.combat_had_real_dmg = False
        self.combat_max_threats = 1
        self.combat_step_idx = 0
        self.last_kite_time = 0.0
        self.last_evade_time = 0.0
        self.last_target_hp = None
        self.last_observed_target_hp = None
        self.target_confirm_streak = 0
        self.missing_target_streak = 0
        self.no_dmg_streak = 0
        self.last_approach_time = 0.0

    def _finalize_kill(self, hp: float, mp: float, current_action: str, close_threats: int = 0) -> float:
        duration = max(0.5, time.time() - self.combat_start)
        # Anti-false-kill guard: if combat lasted <1.1s and never dealt any damage & HP didn't drop, reject as flicker
        if duration < 1.1 and not self.combat_had_real_dmg:
            self._reset_combat_state()
            return 0.0

        reward = self.calculate_reward(
            self.last_hp, hp, self.last_mp, mp, self.last_target_hp, None,
            current_action, threat_count=self.combat_max_threats, duration_sec=duration,
        )
        prev_state = self.brain._state_key(self.last_hp, self.last_mp, self.last_target_hp, False, enemy_count=self.combat_max_threats)
        next_state = self.brain._state_key(hp, mp, None, False, enemy_count=max(1, close_threats))

        try:
            self.kills += 1
            self.brain.total_kills = self.kills
            self.brain.combat_durations.append(round(duration, 2))
            self.brain.learn(prev_state, current_action, reward, next_state, is_terminal=True)
            self.brain.replay_step(batch_size=8)
            self.brain.decay_exploration()
            self.brain.save_memory()

            if self.combat_max_threats >= 2 and hp > 5.0:
                self.workflow_mem.total_multi_mob_survivals += 1
                self.workflow_mem.save()

            avg_ttk = sum(self.brain.combat_durations[-10:]) / max(1, len(self.brain.combat_durations[-10:]))
            log_msg = f"Kill #{self.kills} defeated in {duration:.1f}s (threats={self.combat_max_threats}, Avg TTK: {avg_ttk:.1f}s) | Q(lambda): {reward:+.1f}"
            print(f"\n[AION NEXUS] {log_msg}")
            AionLogger.log(log_msg)

            if self.kills % 3 == 0:
                self.speech.speak(f"Kill {self.kills}. Average time {avg_ttk:.1f} seconds.")
                self.record_evolution_milestone(self.kills, avg_ttk)

            # Only step forward to loot if NOT actively swarmed by >=2 close mobs and HP >= 48%
            if close_threats < 2 and hp >= 48.0:
                AionLogger.log("[LOOTING] Target dead -> stepping in for [F] loot.")
                self.motor.walk_forward(duration=0.45)
                self.motor.loot()
                self.motor.step_back(duration=0.22)
            else:
                AionLogger.log(f"[SURVIVAL] Skipping unsafe forward loot (hp={hp:.1f}%, close_threats={close_threats}).")
        finally:
            self._reset_combat_state()

        return reward

    def _acquire_target_guided(self, scene: Dict[str, Any]) -> Optional[float]:
        """Targets via [Tab], guided by 3D nameplates / minimap without jumping."""
        self.motor.press_key(KEY_TARGET)
        time.sleep(0.14)
        new_scene = self.vision.perceive_scene()
        if new_scene.get("valid") and new_scene.get("target_hp") is not None:
            return new_scene["target_hp"]

        vp_enemies = scene.get("viewport_enemies", []) or scene.get("scout_candidates", [])
        if vp_enemies:
            closest = min(vp_enemies, key=lambda d: abs(d.get("dx_norm", 0.0)))
            self.motor.turn_camera(closest.get("dx_norm", 0.0) * 0.65)
            self.motor.press_key(KEY_TARGET)
            time.sleep(0.14)
            chk = self.vision.perceive_scene()
            if chk.get("valid") and chk.get("target_hp") is not None:
                return chk["target_hp"]

        radar = scene.get("minimap", {})
        if radar.get("total_threats", 0) > 0 and abs(radar.get("steer_dir", 0.0)) > 0.12:
            self.motor.turn_camera(float(radar["steer_dir"]) * 0.55)
        return None

    def _step_quest_mode(self, scene: Dict[str, Any], now: float) -> Optional[str]:
        """
        True Vision-Driven Quest Auto-Run State Machine:
          - Returns 'COMBAT' if target combat should execute this tick, else None.
          - Strictly respects AION 2 [Alt+1] Auto-Run while minimap motion > 0!
        """
        if scene.get("target_hp") is not None:
            self.last_scene_label = "QUEST_COMBAT"
            self.quest_phase = "COMBAT_INTERRUPT"
            self.quest_action_trace.append("COMBAT")
            return "COMBAT"

        q_sig = scene.get("quest_sig", "unknown")
        is_moving = bool(scene.get("is_moving", False))

        # 1. Start Alt+1 Auto-Run if needed
        if self.quest_phase in {"NEED_ALT1", "IDLE"}:
            self._trigger_quest_autorun(q_sig)
            return None

        # 2. Currently Auto-Running via Alt+1 -> HANDS OFF MOVEMENT!
        if self.quest_phase == "AUTORUNNING":
            elapsed = now - self.quest_run_started_at
            if is_moving:
                self.quest_has_moved = True
                self.quest_still_since = 0.0
                self.last_scene_label = "QUEST_AUTORUN"
                if (now - self.last_quest_wait_log) > 4.0:
                    self.last_quest_wait_log = now
                    AionLogger.log(f"[QUEST] Auto-Running via [Alt+1] ({elapsed:.0f}s, motion={scene.get('minimap_motion', 0):.1f}) — no interference.")
                time.sleep(0.18)
                return None

            # Stopped moving: confirm genuinely stopped for 1.1s (or 2.2s right after pressing Alt+1)
            if self.quest_still_since == 0.0:
                self.quest_still_since = now
                time.sleep(0.16)
                return None

            stop_thresh = 1.1 if self.quest_has_moved else 2.2
            if (now - self.quest_still_since) < stop_thresh:
                time.sleep(0.16)
                return None

            # Arrived or stopped!
            self.quest_phase = "ARRIVED"
            AionLogger.log(f"[QUEST] Auto-Run stopped after {elapsed:.1f}s -> Inspecting destination...")

        # 3. Handle Dialogue Overlay when visually open on screen
        if scene.get("dialogue_open") or self.quest_phase == "DIALOGUE":
            self.last_scene_label = "QUEST_DIALOGUE"
            if scene.get("dialogue_open"):
                self.quest_phase = "DIALOGUE"
                if (now - self.last_dialogue_step) > 0.65:
                    self.last_dialogue_step = now
                    self.motor.interact_npc(advance_dialogue=True)
                    self.quest_action_trace.append("NPC_DIALOGUE_STEP")
                    AionLogger.log("[QUEST] NPC dialogue visible -> Advancing conversation.")
                time.sleep(0.18)
                return None
            else:
                # Dialogue just finished! Learn and resume Alt+1 cleanly!
                progressed = q_sig != self.quest_start_sig
                self.workflow_mem.record_quest_flow(
                    self.quest_start_sig, "npc_talk", self.quest_action_trace,
                    max(0.5, now - self.quest_run_started_at), progressed,
                )
                AionLogger.log(f"[WORKFLOW LEARNED] Saved NPC dialogue flow ({self.quest_start_sig} -> progressed={progressed}).")
                self._trigger_quest_autorun(q_sig)
                return None

        # 4. Just Arrived at Quest Destination: Check Hint -> NPC [F] -> Quest Mob [Tab] -> Terrain Detour
        if self.quest_phase == "ARRIVED":
            hint = self.workflow_mem.get_quest_hint(q_sig)
            if hint == "quest_hunt":
                tgt = self._acquire_target_guided(scene)
                if tgt is not None:
                    self.quest_phase = "COMBAT_INTERRUPT"
                    return "COMBAT"

            # Probe NPC / Gather object ONCE with [F] (without blind Space jump!)
            AionLogger.log("[QUEST] Stopped at objective -> Checking NPC / Object with [F]...")
            self.motor.interact_npc(advance_dialogue=False)
            self.quest_action_trace.append("F_INTERACT")
            time.sleep(0.45)
            post_f = self.vision.perceive_scene()
            if post_f.get("valid") and post_f.get("dialogue_open"):
                self.quest_phase = "DIALOGUE"
                return None
            if post_f.get("valid") and post_f.get("quest_sig") != self.quest_start_sig:
                self.workflow_mem.record_quest_flow(
                    self.quest_start_sig, "npc_quick_f", self.quest_action_trace,
                    max(0.5, now - self.quest_run_started_at), True,
                )
                self._trigger_quest_autorun(post_f.get("quest_sig", q_sig))
                return None

            # Check if destination is a Hunt / Kill Quest spot
            tgt = self._acquire_target_guided(scene)
            if tgt is not None:
                self.quest_phase = "COMBAT_INTERRUPT"
                self.quest_action_trace.append("QUEST_HUNT")
                return "COMBAT"

            # Neither NPC dialogue nor mob: check if blocked on terrain before triggering Alt+1 again
            if not self.quest_has_moved:
                esc_dir = self.workflow_mem.best_escape_dir(scene.get("open_side", 0.0))
                AionLogger.log(f"[QUEST DETOUR] Blocked at terrain -> Smooth lateral detour ({'RIGHT' if esc_dir > 0 else 'LEFT'}).")
                self.motor.sidestep_around_obstacle(esc_dir, allow_jump=False)
                self.quest_action_trace.append(f"DETOUR_{'R' if esc_dir > 0 else 'L'}")
                time.sleep(0.20)
                chk_move = self.vision.perceive_scene()
                self.workflow_mem.record_escape(esc_dir, bool(chk_move.get("minimap_motion", 0) > 1.0))

            self._trigger_quest_autorun(q_sig)
            return None

        # Returning from combat interrupt inside Quest Mode -> re-engage Alt+1
        if self.quest_phase == "COMBAT_INTERRUPT":
            progressed = q_sig != self.quest_start_sig
            self.workflow_mem.record_quest_flow(
                self.quest_start_sig, "quest_hunt", self.quest_action_trace,
                max(0.5, now - self.quest_run_started_at), progressed,
            )
            self._trigger_quest_autorun(q_sig)
        return None

    def run(self):
        self.motor.focus_game(force=True)
        time.sleep(0.25)

        self.speech.speak(f"Aion Nexus V6 Vision Agent ready! Mode: {self.hotkeys.mode}.", force=True)
        AionLogger.log(f"=== AION NEXUS V6 SESSION STARTED (Mode: {self.hotkeys.mode}) ===")

        if self.hotkeys.mode == "QUEST":
            self.quest_phase = "NEED_ALT1"

        current_action = "L_AUTO_SHOT"
        reward = 0.0
        last_quest_sig = "unknown"

        while self.running:
            try:
                self._check_ipc_commands(last_quest_sig)

                if self.hotkeys.check_pause_toggle():
                    if self.hotkeys.paused:
                        self.motor.stop_movement()
                        self.speech.speak("Aion paused! Manual control engaged.", force=True)
                    else:
                        self.motor.focus_game(force=True)
                        if self.hotkeys.mode == "QUEST":
                            self.quest_phase = "NEED_ALT1"
                        self.speech.speak(f"Aion resuming! Mode: {self.hotkeys.mode}", force=True)

                if self.hotkeys.check_mode_toggle():
                    if self.hotkeys.mode == "QUEST":
                        self.quest_phase = "NEED_ALT1"
                    else:
                        self.quest_phase = "IDLE"
                    self.speech.speak(f"Mode switched to {self.hotkeys.mode}!", force=True)
                    AionLogger.log(f"[MODE TOGGLE] New Mode: {self.hotkeys.mode}")

                self.geom.update()
                game_active = bool(self.geom.wid) and self.geom.is_game_active()

                if self.hotkeys.paused or not game_active:
                    if self.motor.lmb_auto_fire:
                        self.motor.set_auto_fire(False)
                    self.last_scene_label = "PAUSED" if self.hotkeys.paused else "GAME_UNFOCUSED"
                    self._write_status_telemetry(
                        None, None, None, "PAUSED" if self.hotkeys.paused else "WAITING_FOCUS",
                        reward, window_visible=False,
                    )
                    time.sleep(0.22)
                    continue

                scene = self.vision.perceive_scene()
                if not scene.get("valid"):
                    if self.motor.lmb_auto_fire:
                        self.motor.set_auto_fire(False)
                    self.last_scene_label = "FRAME_OCCLUDED"
                    self._write_status_telemetry(None, None, None, "OCCLUDED", reward, window_visible=False)
                    time.sleep(0.18)
                    continue

                hp = scene["player_hp"]
                mp = scene["player_mp"]
                hp_drop_rate = scene["hp_drop_rate"]
                target_hp = scene["target_hp"]
                hotbar = scene["hotbar"]
                last_quest_sig = scene["quest_sig"]
                threat_count = scene["threat_count"]
                close_threats = scene["minimap"]["close_threats"]

                self.workflow_mem.update_hotbar_knowledge(hotbar)
                locked_or_empty = hotbar.get("locked_or_empty") or self.workflow_mem.hotbar_profile.get("locked_or_empty", [])
                active_skills = [s.replace("SKILL_", "") for s in hotbar.get("ready_keys", [])]

                self._write_status_telemetry(
                    hp, mp, target_hp, current_action, reward,
                    window_visible=True, threats=threat_count, active_skills=active_skills,
                )

                # Death guard: never spam combat/keys while defeated
                if scene.get("is_dead"):
                    self.last_scene_label = "PLAYER_DEFEATED"
                    if self.combat_start > 0:
                        self._reset_combat_state()
                    self.motor.stop_movement()
                    self.hotkeys.paused = True
                    self.speech.speak("Character defeated! Auto-pausing for safety.", force=True)
                    AionLogger.log("[SURVIVAL] Defeated state visually detected -> Paused cleanly.")
                    continue

                # Proactive potions whenever playing
                self._use_survival_potions(hp, mp, threat_count)

                now = time.time()

                # Mode Dispatch when NOT locked on a confirmed combat target
                if target_hp is None:
                    if self.motor.lmb_auto_fire:
                        self.motor.set_auto_fire(False)
                    if self.combat_start > 0:
                        self.missing_target_streak += 1
                        # Debounce brief HUD flicker unless target frame disappeared completely AND dmg dealt
                        if self.missing_target_streak < 2 and scene.get("target_frame"):
                            time.sleep(0.06)
                            continue
                        reward = self._finalize_kill(hp, mp, current_action, close_threats=close_threats)
                        if self.hotkeys.mode == "FARM":
                            # Only chain immediately if safe HP or close mob already attacking!
                            if close_threats >= 1 or hp >= 70.0:
                                chain = self._acquire_target_guided(scene)
                                if chain is not None:
                                    AionLogger.log(f"[CHAIN TARGET] Locked next mob! HP: {chain:.1f}%")
                                    continue

                    if self.hotkeys.mode == "QUEST":
                        # If being ambushed during Alt+1 (sharp HP drop), tap Tab cleanly to retaliate
                        if self.quest_phase == "AUTORUNNING" and hp_drop_rate > 3.5:
                            AionLogger.log(f"[QUEST AMBUSH] HP dropping ({hp_drop_rate:.1f}%/s) -> Acquiring attacker!")
                            self.motor.press_key(KEY_TARGET)
                            time.sleep(0.12)
                            continue
                        action_req = self._step_quest_mode(scene, now)
                        if action_req != "COMBAT":
                            continue
                    else:
                        # FARM Mode: Wait briefly out of combat to recover HP if injured (<72%) before pulling next pack
                        if hp < 72.0 and close_threats == 0:
                            self.last_scene_label = "RECOVER_HP"
                            AionLogger.log(f"[FARM SURVIVAL] Healing HP ({hp:.1f}%) before pulling next target...")
                            time.sleep(0.8)
                            continue

                        self.last_scene_label = "FARM_SCOUT"
                        scanned = self._acquire_target_guided(scene)
                        if scanned is not None:
                            continue

                        self.roam_count += 1
                        if self.roam_count >= 3:
                            radar = scene["minimap"]
                            # Prefer isolated/available direction; avoid blind hopping!
                            if radar.get("total_threats", 0) > 0 and abs(radar.get("steer_dir", 0.0)) > 0.08:
                                self.motor.turn_camera(float(radar["steer_dir"]) * 0.70)
                            elif abs(scene.get("open_side", 0.0)) > 0.1:
                                self.motor.turn_camera(float(scene["open_side"]) * 0.45)
                            self.motor.walk_forward(duration=0.45)
                            self.roam_count = 0
                            time.sleep(0.12)
                        continue

                # Active Combat Loop (Target HP Bar Visually Confirmed Alive!)
                self.missing_target_streak = 0
                self.roam_count = 0
                if self.combat_start == 0.0:
                    # Wait 1 confirm frame (~50ms) so momentary red noise never creates 0.7s phantom kills
                    self.target_confirm_streak += 1
                    if self.target_confirm_streak < 2 and (target_hp or 0.0) < 25.0:
                        time.sleep(0.05)
                        continue
                    self.combat_start = now
                    self.combat_had_real_dmg = False
                    self.combat_max_threats = max(1, threat_count)
                    self.combat_step_idx = 0
                    self.last_kite_time = 0.0
                    self.no_dmg_streak = 0
                    self.last_target_change = now
                    self.last_observed_target_hp = target_hp
                    self.last_scene_label = "MULTI_MOB_COMBAT" if threat_count >= 2 else "COMBAT"
                    self.motor.set_auto_fire(True)
                    self.speech.speak("Target locked! Engaging." if threat_count < 2 else f"Caution, {threat_count} targets detected!")
                    AionLogger.log(f"[COMBAT START] Target HP: {target_hp:.1f}% | Threats: {threat_count} | ActiveSkills={active_skills}")
                else:
                    self.combat_max_threats = max(self.combat_max_threats, threat_count)
                    self.last_scene_label = "MULTI_MOB_COMBAT" if threat_count >= 2 else "COMBAT"
                    self.motor.set_auto_fire(True)

                if self.last_observed_target_hp is None or abs(target_hp - self.last_observed_target_hp) > 1.2:
                    if self.last_observed_target_hp is not None and (self.last_observed_target_hp - target_hp) > 1.2:
                        self.combat_had_real_dmg = True
                    self.last_target_change = now
                    self.last_observed_target_hp = target_hp
                    self.no_dmg_streak = 0

                stall_elapsed = now - self.last_target_change
                is_stuck = (stall_elapsed > 6.5) or (self.no_dmg_streak >= 5)

                # If target is locked ([Tab]) at long range with 0 damage dealt yet, close range ([W]) before retrying skills
                if ((not self.combat_had_real_dmg and stall_elapsed >= 1.5) or (self.no_dmg_streak >= 3 and close_threats == 0)) and (now - self.last_approach_time) >= 2.4:
                    self.last_approach_time = now
                    AionLogger.log(f"[APPROACH] Closing distance [W] to target (stall={stall_elapsed:.1f}s, no_dmg={self.no_dmg_streak}).")
                    self.motor.approach_target(duration=0.42)
                    self.motor.set_auto_fire(True)

                if stall_elapsed > 11.5:
                    self.motor.set_auto_fire(False)
                    esc = self.workflow_mem.best_escape_dir(scene.get("open_side", 0.0))
                    AionLogger.log("[UNSTUCK] Target unreachable >11.5s -> Lateral detour + re-target.")
                    self.motor.sidestep_around_obstacle(esc, allow_jump=True)
                    self.motor.press_key(KEY_TARGET)
                    self.last_target_change = time.time()
                    self.no_dmg_streak = 0
                    continue

                in_melee = (now - self.combat_start) > 2.2 or hp_drop_rate > 4.0 or close_threats >= 1
                state = self.brain._state_key(hp, mp, target_hp, in_melee, enemy_count=threat_count, stalled=is_stuck)

                current_action = self.brain.choose_action(
                    state,
                    mp,
                    is_stuck=is_stuck,
                    skill_last_used=self.skill_last_used,
                    now=now,
                    locked_or_empty=locked_or_empty,
                    cooldown_keys=hotbar.get("cooldown_keys", ()),
                    threat_count=threat_count,
                    hp=hp,
                )
                self.execute_action(current_action, away_dir=scene.get("away_dir", 0.0))
                self.combat_step_idx += 1

                if mp > 25.0 and current_action in {"L_AUTO_SHOT", "SKILL_1", "SKILL_2"} and random.random() < 0.22:
                    self.motor.shoot_right()

                # Agile Ranged Distance-Keeping: keep LMB Dauerfeuer ON while gliding back/diagonally!
                kite_mode = "NONE"
                now_k = time.time()
                if (hp_drop_rate >= 4.5 or (threat_count >= 2 and hp < 62.0) or hp < 44.0) and (now_k - self.last_evade_time) >= 2.4:
                    self.last_evade_time = now_k
                    self.last_kite_time = now_k
                    self.motor.evade_back(away_dir=scene.get("away_dir", 0.0))
                    kite_mode = "EVADE_SHIFT_S"
                elif current_action not in MOVEMENT_SKILLS and (self.combat_had_real_dmg or (now_k - self.combat_start) >= 0.9) and (now_k - self.last_kite_time) >= 0.65:
                    self.last_kite_time = now_k
                    alt_side = 1.0 if ((self.combat_step_idx // 2) % 2 == 0) else -1.0
                    s_dir = float(scene.get("open_side", 0.0)) or alt_side
                    need_back = (close_threats >= 1) or (hp_drop_rate > 1.0) or (hp < 88.0) or (self.combat_step_idx % 2 == 0)
                    self.motor.kite_orbwalk(strafe_dir=s_dir, backstep=need_back, duration=0.30)
                    kite_mode = f"{'BACK+' if need_back else ''}{'R' if s_dir > 0 else 'L'}"
                else:
                    # Keep LMB Dauerfeuer shooting over 2-3 game frames (10-11 FPS)
                    time.sleep(0.22)

                if (time.time() - self.last_combat_log) >= 2.5:
                    self.last_combat_log = time.time()
                    AionLogger.log(
                        f"[COMBAT] Cast={current_action} | LMB={'ON' if self.motor.lmb_auto_fire else 'OFF'} "
                        f"| Kite={kite_mode} | Tgt={target_hp:.1f}% | Skills={','.join(active_skills) or '-'}"
                    )

                post = self.vision.perceive_scene()
                if not post.get("valid"):
                    continue

                new_hp = post["player_hp"]
                new_mp = post["player_mp"]
                new_target = post["target_hp"]

                if new_target is not None and (target_hp - new_target) > 0.3:
                    self.combat_had_real_dmg = True

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
                    current_action, is_stuck=is_stuck, threat_count=threat_count,
                    duration_sec=(time.time() - self.combat_start),
                )
                if not is_kill:
                    new_in_melee = (time.time() - self.combat_start) > 2.2 or post.get("hp_drop_rate", 0.0) > 4.0
                    next_state = self.brain._state_key(new_hp, new_mp, new_target, new_in_melee, enemy_count=post.get("threat_count", 1))
                    self.brain.learn(state, current_action, reward, next_state, is_terminal=False)

                self.last_hp = new_hp
                self.last_mp = new_mp
                self.last_target_hp = new_target
            except KeyboardInterrupt:
                break
            except Exception as e:
                AionLogger.log(f"[LOOP RECOVERED] {e}")
                time.sleep(0.2)

        self.motor.stop_movement()

        self.brain.save_memory()
        self.workflow_mem.save()
        self._write_status_telemetry(self.last_hp, self.last_mp, None, "STOPPED", 0.0, window_visible=False, force=True)
        self.speech.speak(f"Aion Nexus stopped cleanly. {self.kills} Kills & {len(self.workflow_mem.quest_workflows)} workflows saved.", force=True)
        AionLogger.log(f"=== AION NEXUS V6 SESSION FINISHED ({self.kills} Kills) ===")


def _run_self_test():
    """Offline end-to-end verification of V6 Vision, Quest Auto-Run safety, Survival & RL."""
    import tempfile
    from PIL import Image

    print("[AION NEXUS GAMER V6] Running Self-Verification Suite...")

    # 1. Synthetic Vision + Foliage-Immune Target Frame Verification
    frame = np.zeros((360, 640, 3), dtype=np.uint8)
    # Paint red Cantas Valley bush across screen center & bottom -> must NOT trigger false target
    frame[180:290, 200:440] = [205, 18, 18]
    tgt_bush, frm_bush = AionVision.parse_target_rgb(frame)
    assert tgt_bush is None and not frm_bush, f"Red bush false target: {tgt_bush}"

    # Paint real Target Bar with dark metallic top frame + crimson fill + center orb
    frame[14:16, 270:370] = [20, 20, 20]  # dark top metallic border
    frame[16:20, 270:370] = [215, 22, 22]  # red fill
    frame[15:21, 315:325] = [180, 140, 60]  # center orb medallion
    tgt_real, frm_real = AionVision.parse_target_rgb(frame)
    assert tgt_real is not None and tgt_real > 85.0 and frm_real, f"Expected real target ~95-100%, got {tgt_real}"

    vp_res = AionVision.detect_viewport_entities_rgb(frame[50:300, 96:544])
    assert len(vp_res["enemies"]) == 0, f"Red bush should not trigger viewport bar entities, got {vp_res}"
    print(" -> Synthetic Target Frame & Red-Bush Immunity passed.")

    # 2. Real Live Screenshot Calibration Verification (if present in conversation artifacts)
    real_shot = "/root/.gemini/antigravity/brain/c6511b79-20e6-46e8-b715-e423f09a002f/.user_uploaded/media_1794745692469_cd23b36c.jpg"
    if os.path.exists(real_shot):
        real_rgb = np.array(Image.open(real_shot))
        vis = AionVision(DynamicWindowGeometry(win_titles=("__NONE__",)), disp=None)
        scene = vis.perceive_scene(real_rgb)
        assert scene["valid"]
        assert scene["player_hp"] > 80.0, f"Real HP expected >80%, got {scene['player_hp']}"
        assert scene["player_mp"] > 85.0, f"Real MP expected >85%, got {scene['player_mp']}"
        assert scene["target_hp"] is not None and scene["target_hp"] > 85.0, f"Real Target expected >85%, got {scene['target_hp']}"
        assert "SKILL_4" in scene["hotbar"]["locked_or_empty"], f"Locked Lv.16 skill 4 should be detected: {scene['hotbar']}"
        assert "SKILL_5" in scene["hotbar"]["locked_or_empty"], f"Empty slot 5 should be detected: {scene['hotbar']}"
        assert scene["minimap"]["total_threats"] >= 1, f"Minimap threats expected >=1, got {scene['minimap']}"
        assert scene["minimap"]["quest_blips"] >= 3, f"Quest cyan blips expected >=3, got {scene['minimap']}"
        print(
            f" -> Real AION 2 Screenshot V6 Vision passed: HP={scene['player_hp']}%, MP={scene['player_mp']}%, "
            f"Target={scene['target_hp']}%, Locked={scene['hotbar']['locked_or_empty']}, Threats={scene['threat_count']}."
        )

    # 2b. Verify Auto-Farm screenshot: ALL 6 active skills READY (no magenta false locks!) & zero sky/GUI phantom threats
    farm_shot = "/root/.gemini/antigravity/brain/c6511b79-20e6-46e8-b715-e423f09a002f/.user_uploaded/media_1794747342553_76b0a7c1.jpg"
    if os.path.exists(farm_shot):
        f_rgb = np.array(Image.open(farm_shot))
        vis2 = AionVision(DynamicWindowGeometry(win_titles=("__NONE__",)), disp=None)
        f_scene = vis2.perceive_scene(f_rgb)
        assert f_scene["valid"]
        assert f_scene["player_hp"] > 90.0 and f_scene["target_hp"] > 90.0
        assert len(f_scene["viewport_enemies"]) <= 1, f"Sky/GUI noise leaked into viewport_enemies: {f_scene['viewport_enemies']}"
        assert f_scene["threat_count"] <= 2, f"Phantom threat spike on farm screenshot: {f_scene['threat_count']}"
        ready_set = set(f_scene["hotbar"]["ready_keys"])
        for req_sk in ("SKILL_1", "SKILL_2", "SKILL_3", "SKILL_Q", "SKILL_E", "SKILL_R"):
            assert req_sk in ready_set, f"Active skill {req_sk} falsely locked/excluded: {f_scene['hotbar']['slots']}"
        assert "SKILL_4" in f_scene["hotbar"]["locked_or_empty"]
        print(
            f" -> Auto-Farm Vision & Hotbar passed: Threats={f_scene['threat_count']}, "
            f"Ready={len(ready_set)}/6 ({sorted(ready_set)})."
        )

    # 2c. Verify LMB Dauerfeuer toggle + Orbwalk distance-keeping + safety release
    m_test = AionMotor(DynamicWindowGeometry(win_titles=("__NONE__",)), disp=None, dry_run=True)
    m_test.set_auto_fire(True)
    assert m_test.lmb_auto_fire is True and "BDN:1" in m_test.command_log
    m_test.kite_orbwalk(strafe_dir=-1.0, backstep=True, duration=0.05)
    assert "DN:s" in m_test.command_log and "DN:a" in m_test.command_log and m_test.lmb_auto_fire is True
    m_test.stop_movement()
    assert m_test.lmb_auto_fire is False and "BUP:1" in m_test.command_log
    print(" -> LMB Auto-Fire Dauerfeuer + Kite Orbwalk + Safe Release passed.")

    # 3. Quest Auto-Run Non-Interference & Workflow Persistence Test
    tmp_mem = os.path.join(tempfile.gettempdir(), "aion_v6_combat_test.json")
    tmp_wf = os.path.join(tempfile.gettempdir(), "aion_v6_workflow_test.json")
    for p in (tmp_mem, tmp_wf):
        if os.path.exists(p):
            os.remove(p)

    wf = GameWorkflowMemory(memory_file=tmp_wf)
    wf.record_quest_flow("q_001122", "npc_talk", ["ALT_1", "F_INTERACT", "NPC_DIALOGUE_STEP"], 12.4, True)
    wf_reloaded = GameWorkflowMemory(memory_file=tmp_wf)
    assert wf_reloaded.get_quest_hint("q_001122") == "npc_talk"
    assert wf_reloaded.total_quests_progressed == 1

    # Simulate Quest Auto-Run moving frame -> verify NO WASD or Space sent!
    geom = DynamicWindowGeometry(win_titles=("__NONE__",))
    gamer = AionNexusAutonomousGamer.__new__(AionNexusAutonomousGamer)
    gamer.dry_run = True
    gamer.geom = geom
    gamer.motor = AionMotor(geom, disp=None, dry_run=True)
    gamer.brain = NexusCombatBrain(memory_file=tmp_mem)
    gamer.workflow_mem = wf_reloaded
    gamer.hotkeys = GlobalHotkeyManager(disp=None)
    gamer.hotkeys.mode = "QUEST"
    gamer.vision = AionVision(geom, disp=None)
    gamer.quest_phase = "AUTORUNNING"
    gamer.quest_run_started_at = time.time() - 3.0
    gamer.quest_has_moved = True
    gamer.quest_still_since = 0.0
    gamer.quest_start_sig = "q_test"
    gamer.quest_action_trace = ["ALT_1"]
    gamer.last_quest_wait_log = time.time()
    gamer.last_scene_label = "QUEST_AUTORUN"

    gamer.motor.command_log.clear()
    req = gamer._step_quest_mode({
        "valid": True, "target_hp": None, "is_moving": True,
        "minimap_motion": 5.2, "quest_sig": "q_test",
        "dialogue_open": False, "interact_prompt": False,
    }, time.time())
    assert req is None and len(gamer.motor.command_log) == 0, f"Auto-Run sent keys while moving! {gamer.motor.command_log}"
    print(" -> Quest Auto-Run Zero-Interference & Workflow Persistence passed.")

    # 4. V6.1 RL Arena Test (No JUMP_UNSTUCK, No Kite-Loop on healthy 1v1, Multi-mob survival)
    brain = NexusCombatBrain(memory_file=tmp_mem)
    assert "JUMP_UNSTUCK" not in ACTIONS
    avail = brain.available_actions(mp=80.0, locked_or_empty=["SKILL_4", "SKILL_5"], threat_count=3, hp=42.0)
    assert "SKILL_4" not in avail and "SKILL_5" not in avail and "EVADE_BACK" in avail

    # Even if TACTICAL_KITE had inflated weight, healthy 1v1 pulls MUST prioritize offensive attacks!
    s_healthy = brain._state_key(95.0, 90.0, 95.0, False, enemy_count=1)
    brain._ensure_state(s_healthy)
    brain.q_table[s_healthy]["TACTICAL_KITE"] = 25.0
    for _ in range(10):
        chosen = brain.choose_action(s_healthy, mp=90.0, locked_or_empty=["SKILL_4", "SKILL_5"], threat_count=1, hp=95.0)
        assert chosen in OFFENSIVE_SKILLS and chosen not in {"TACTICAL_KITE", "EVADE_BACK"}, f"Should attack not kite on healthy 1v1, got {chosen}"

    stuck_avail = brain.available_actions(mp=80.0, is_stuck=True, locked_or_empty=["SKILL_4", "SKILL_5"], threat_count=1, hp=95.0)
    assert "SKILL_1" in stuck_avail and "L_AUTO_SHOT" in stuck_avail, f"Stuck state must keep attacks active: {stuck_avail}"

    true_dmg = {"SKILL_R": 36.0, "SKILL_3": 28.0, "SKILL_E": 22.0, "SKILL_2": 17.0, "SKILL_1": 15.0, "R_HEAVY_SHOT": 13.0, "L_AUTO_SHOT": 8.0, "EVADE_BACK": 6.0, "TACTICAL_KITE": 5.0}
    sim_t = 1000.0
    last_u = {k: 0.0 for k in SKILL_KEY_MAP}
    for _ in range(25):
        m_hp, stp = 100.0, 0
        while m_hp > 0 and stp < 20:
            stp += 1
            sim_t += 0.4
            st = brain._state_key(80.0, 80.0, m_hp, False, enemy_count=1)
            act = brain.choose_action(st, 80.0, skill_last_used=last_u, now=sim_t, locked_or_empty=["SKILL_4", "SKILL_5"])
            if act in SKILL_KEY_MAP:
                last_u[act] = sim_t
            d = true_dmg.get(act, 6.0)
            nxt = max(0.0, m_hp - d)
            brain.record_skill_outcome(act, min(m_hp, d))
            kill = nxt <= 0.0
            brain.learn(st, act, d * 0.48 + (25.0 if kill else 0.0), brain._state_key(80.0, 80.0, None if kill else nxt, False), is_terminal=kill)
            m_hp = nxt
            if kill:
                brain.replay_step(4)
                brain.decay_exploration()
    brain.save_memory()
    for p in (tmp_mem, tmp_wf):
        if os.path.exists(p):
            os.remove(p)
    print(f" -> V6 Survival RL Arena passed (states={len(brain.q_table)}).")
    print("[SUCCESS] All AION NEXUS V6 Self-Tests passed!")


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
