#!/usr/bin/env python3
# ==============================================================================
# AION NEXUS — AUTONOMOUS MULTI-DOMAIN REINFORCEMENT LEARNING AGENT (AION 2)
# ==============================================================================
# Sovereign AI agent that autonomously plays AION 2, learns from:
#   1. Combat: Q-Learning optimal rotations (L-Barrage, R-Heavy, Skills 1-5, Q, E, R).
#      - Continuous Left-Click primary fire during movement and kiting.
#      - Heavy Right-Click bow shots when MP > 20%.
#      - Full skill bar weaving (Keys 1, 2, 3, 4, 5, Q, E, R).
#   2. Movements: Circle-kiting (Strafing A/D) while firing arrows,
#      tactical backward kiting (S) while firing, and collision unstuck (Jump+Leap).
#   3. Questing & Aggressive Mob Hunting:
#      - Continuous Tab-Targeting in both Quest and Farm modes.
#      - Instant kill-chain targeting (immediately acquiring next pack mob upon kill).
#      - Auto-clicking Quest Tracker, navigating to objectives, talking with NPCs ([F]/[Space]).
#   4. Map Roaming & Exploration: Natural search trajectories across terrain.
#   5. Telemetry & GUI Integration:
#      - Status JSON: '/tmp/aion_nexus_status.json'
#      - Command IPC: '/tmp/aion_nexus_cmd.txt'
#      - Live Log:    '/tmp/aion_nexus_log.txt'
#   6. Hotkeys: [F8] Toggle Mode, [F10]/[Pause] Global Pause
# ==============================================================================

import os
import sys
import time
import json
import random
import re
import subprocess
from typing import Dict, Any, Optional, Tuple, List
from Xlib import display, X

AI_DIR = "/root/coding/AI_Desktop"
MEINOS_DIR = "/root/coding/MeinOS"
sys.path.extend([AI_DIR, MEINOS_DIR])

try:
    from aion_episodic_graph import EpisodicKnowledgeGraph
    from aion_living_book import LivingMemoryBook
except ImportError:
    EpisodicKnowledgeGraph = None
    LivingMemoryBook = None

AION_WINDOW_NAME = "AION2"
COMBAT_MEMORY_FILE = os.path.join(MEINOS_DIR, "aion_nexus_combat_memory.json")
STATUS_FILE = "/tmp/aion_nexus_status.json"
CMD_FILE = "/tmp/aion_nexus_cmd.txt"
LOG_FILE = "/tmp/aion_nexus_log.txt"

# Normalized HUD Ratios (Resolution & Borderless Window Independent)
NORM_HP_X_START = 0.3844
NORM_HP_X_END = 0.4813
NORM_HP_Y = 0.9028

NORM_MP_X_START = 0.5182
NORM_MP_X_END = 0.6130
NORM_MP_Y = 0.9055

NORM_TGT_X_START = 0.4234
NORM_TGT_X_END = 0.6755
NORM_TGT_Y = 0.0491

NORM_QUEST_X = 0.9220
NORM_QUEST_Y = 0.2710

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

# Actions in Multi-Domain Q-Space (Every Action Keeps Arrows Flying!)
ACTIONS = [
    "L_AUTO_SHOT",    # 0: Continuous primary bow volley (3 rapid shots)
    "R_HEAVY_SHOT",   # 1: Heavy charged shot (mana throttled) + LMB weave
    "SKILL_1",        # 2: Bow Skill 1 (Primary Burst) + LMB weave
    "SKILL_2",        # 3: Bow Skill 2 (Piercing Arrow) + LMB weave
    "SKILL_3",        # 4: Bow Skill 3 (Rapid Arrow Volley) + LMB weave
    "SKILL_4",        # 5: Bow Skill 4 (Stun / Heavy Impact) + LMB weave
    "SKILL_5",        # 6: Bow Skill 5 (Inferno / Fiery Arrow) + LMB weave
    "SKILL_Q",        # 7: Bow Skill Q (Runic Shot) + LMB weave
    "SKILL_E",        # 8: Bow Skill E (Snare / Piercing) + LMB weave
    "SKILL_R",        # 9: Bow Skill R (Burst Ultimate) + LMB weave
    "TACTICAL_KITE",  # 10: Step backward with [S] WHILE firing continuous arrows
    "STRAFE_LEFT",    # 11: Circle-kite left with [A] WHILE firing continuous arrows
    "STRAFE_RIGHT",   # 12: Circle-kite right with [D] WHILE firing continuous arrows
    "JUMP_UNSTUCK",   # 13: Forward leap + jump [Space] WHILE firing mid-air arrow
]


class AionLogger:
    @staticmethod
    def log(msg: str):
        formatted = f"[{time.strftime('%H:%M:%S')}] {msg}\n"
        try:
            with open(LOG_FILE, "a", encoding="utf-8") as f:
                f.write(formatted)
        except Exception:
            pass


class AionSpeechBridge:
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
            with open(self.bridge_pipe, "a") as f:
                f.write(formatted)
        except Exception:
            pass


class DynamicWindowGeometry:
    def __init__(self, win_name: str = AION_WINDOW_NAME):
        self.win_name = win_name
        self.wid = None
        self.ax = 0
        self.ay = 0
        self.w = 1920
        self.h = 1080
        self.last_check = 0.0
        self.update()

    def update(self, force: bool = False) -> bool:
        now = time.time()
        if not force and (now - self.last_check) < 2.0 and self.wid:
            return True
        self.last_check = now
        try:
            out = subprocess.check_output(["xdotool", "search", "--name", self.win_name], text=True).strip()
            if out:
                self.wid = int(out.splitlines()[0])
                winfo = subprocess.check_output(["xwininfo", "-id", str(self.wid)], text=True)
                self.ax = int(re.search(r"Absolute upper-left X:\s*(-?\d+)", winfo).group(1))
                self.ay = int(re.search(r"Absolute upper-left Y:\s*(-?\d+)", winfo).group(1))
                self.w = int(re.search(r"Width:\s*(\d+)", winfo).group(1))
                self.h = int(re.search(r"Height:\s*(\d+)", winfo).group(1))
                return True
        except Exception:
            pass
        return False


class AionVision:
    def __init__(self, geom: DynamicWindowGeometry):
        self.geom = geom
        self.disp = display.Display(":0")
        self.root = self.disp.screen().root

    def get_player_hp(self) -> float:
        self.geom.update()
        hp_x = self.geom.ax + int(NORM_HP_X_START * self.geom.w)
        hp_w = max(10, int((NORM_HP_X_END - NORM_HP_X_START) * self.geom.w))
        hp_y = self.geom.ay + int(NORM_HP_Y * self.geom.h)
        try:
            img = self.root.get_image(hp_x, hp_y, hp_w, 4, X.ZPixmap, 0xffffffff)
            data = img.data
            red_count = 0
            for i in range(0, len(data), 4):
                b, g, r = data[i], data[i+1], data[i+2]
                if r > 120 and g < 70 and b < 70:
                    red_count += 1
            return min(100.0, (red_count / (hp_w * 4)) * 100.0)
        except Exception:
            return 0.0

    def get_player_mp(self) -> float:
        self.geom.update()
        mp_x = self.geom.ax + int(NORM_MP_X_START * self.geom.w)
        mp_w = max(10, int((NORM_MP_X_END - NORM_MP_X_START) * self.geom.w))
        mp_y = self.geom.ay + int(NORM_MP_Y * self.geom.h)
        try:
            img = self.root.get_image(mp_x, mp_y, mp_w, 3, X.ZPixmap, 0xffffffff)
            data = img.data
            blue_count = 0
            for i in range(0, len(data), 4):
                b, g, r = data[i], data[i+1], data[i+2]
                if b > 120 and r < 70:
                    blue_count += 1
            return min(100.0, (blue_count / (mp_w * 3)) * 100.0)
        except Exception:
            return 0.0

    def get_target_hp(self) -> Optional[float]:
        self.geom.update()
        tgt_x = self.geom.ax + int(NORM_TGT_X_START * self.geom.w)
        tgt_w = max(20, int((NORM_TGT_X_END - NORM_TGT_X_START) * self.geom.w))
        tgt_y = self.geom.ay + int(NORM_TGT_Y * self.geom.h)
        try:
            total_red = 0
            for y_off in [-1, 0, 1, 2]:
                img = self.root.get_image(tgt_x, tgt_y + y_off, tgt_w, 1, X.ZPixmap, 0xffffffff)
                data = img.data
                for i in range(0, len(data), 4):
                    b, g, r = data[i], data[i+1], data[i+2]
                    if r > 140 and g < 65 and b < 65:
                        total_red += 1
            total_sampled = tgt_w * 4
            if total_red < 15:
                return None
            pct = (total_red / total_sampled) * 100.0 * 2.5
            return max(1.0, min(100.0, pct))
        except Exception:
            return None


class AionMotor:
    def __init__(self, geom: DynamicWindowGeometry):
        self.geom = geom

    def focus_game(self):
        if not self.geom.wid:
            self.geom.update(force=True)
        if self.geom.wid:
            try:
                active = subprocess.check_output(["xdotool", "getactivewindow"], text=True, stderr=subprocess.DEVNULL).strip()
                if active:
                    if int(active) == int(self.geom.wid):
                        return
                    # Avoid stealing focus when user interacts with GUI window
                    active_name = subprocess.check_output(["xdotool", "getwindowname", active], text=True, stderr=subprocess.DEVNULL).strip()
                    if "AION 2 — NEURAL" in active_name or "NEURAL GAMING" in active_name:
                        return
                subprocess.run(["xdotool", "windowactivate", str(self.geom.wid)],
                               check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception:
                pass

    def rapid_shoot_left(self, count: int = 3, delay: float = 0.05):
        """Continuous primary bow shot volley."""
        try:
            for _ in range(count):
                subprocess.run(["xdotool", "click", "1"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if delay > 0:
                    time.sleep(delay)
        except Exception:
            pass

    def shoot_right(self):
        """Heavy charged bow shot."""
        try:
            subprocess.run(["xdotool", "mousedown", "3"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.08)
            subprocess.run(["xdotool", "mouseup", "3"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def press_key(self, key: str, delay: int = 65):
        try:
            subprocess.run(["xdotool", "key", "--delay", str(delay), key], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def strafe_left_and_shoot(self, duration: float = 0.30):
        """Circle-kites left with [A] while continuously firing arrows."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_LEFT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.rapid_shoot_left(count=3, delay=0.06)
            subprocess.run(["xdotool", "keyup", KEY_LEFT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def strafe_right_and_shoot(self, duration: float = 0.30):
        """Circle-kites right with [D] while continuously firing arrows."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_RIGHT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.rapid_shoot_left(count=3, delay=0.06)
            subprocess.run(["xdotool", "keyup", KEY_RIGHT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def kite_back_and_shoot(self, duration: float = 0.32):
        """Steps backward with [S] to maintain distance while firing arrows."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.rapid_shoot_left(count=3, delay=0.06)
            subprocess.run(["xdotool", "keyup", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def jump_shot_unstuck(self):
        """Performs a leap forward + jump to clear terrain collision while firing in mid-air."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run(["xdotool", "key", KEY_JUMP], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.rapid_shoot_left(count=2, delay=0.05)
            time.sleep(0.2)
            subprocess.run(["xdotool", "keyup", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def walk_forward(self, duration: float = 0.5):
        try:
            subprocess.run(["xdotool", "keydown", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def stop_movement(self):
        try:
            subprocess.run(["xdotool", "keyup", KEY_FORWARD, "keyup", KEY_BACKWARD, "keyup", KEY_LEFT, "keyup", KEY_RIGHT],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def loot(self):
        for _ in range(3):
            self.press_key(KEY_LOOT, delay=70)
            time.sleep(0.18)

    def click_quest_tracker(self):
        if not self.geom.wid:
            self.geom.update(force=True)
        if not self.geom.wid:
            return
        qx = int(NORM_QUEST_X * self.geom.w)
        qy = int(NORM_QUEST_Y * self.geom.h)
        cx = int(self.geom.w * 0.5)
        cy = int(self.geom.h * 0.5)
        try:
            subprocess.run(["xdotool", "mousemove", "--window", str(self.geom.wid), str(qx), str(qy), "click", "1"],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.08)
            subprocess.run(["xdotool", "mousemove", "--window", str(self.geom.wid), str(cx), str(cy)],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def interact_npc(self):
        self.press_key(KEY_INTERACT, delay=80)
        time.sleep(0.15)
        self.press_key(KEY_DIALOGUE_ADVANCE, delay=60)


class GlobalHotkeyManager:
    def __init__(self):
        self.disp = display.Display(":0")
        self.pause_keycodes = [76, 127, 75]  # F10, Pause, F9
        self.mode_keycodes = [74]           # F8
        self.prev_pause_down = False
        self.prev_mode_down = False
        self.paused = False
        self.mode = "QUEST"

    def check_pause_toggle(self) -> bool:
        try:
            keymap = self.disp.query_keymap()
            is_down = any(bool(keymap[kc // 8] & (1 << (kc % 8))) for kc in self.pause_keycodes)
            if is_down and not self.prev_pause_down:
                self.paused = not self.paused
                self.prev_pause_down = True
                return True
            elif not is_down:
                self.prev_pause_down = False
        except Exception:
            pass
        return False

    def check_mode_toggle(self) -> bool:
        try:
            keymap = self.disp.query_keymap()
            is_down = any(bool(keymap[kc // 8] & (1 << (kc % 8))) for kc in self.mode_keycodes)
            if is_down and not self.prev_mode_down:
                self.mode = "FARM" if self.mode == "QUEST" else "QUEST"
                self.prev_mode_down = True
                return True
            elif not is_down:
                self.prev_mode_down = False
        except Exception:
            pass
        return False


class NexusCombatBrain:
    """Multi-Domain Reinforcement Learning Policy (Offense, Circle Kiting, Unstuck, Quest)."""
    def __init__(self):
        self.alpha = 0.15
        self.gamma = 0.85
        self.epsilon = 0.30
        self.epsilon_min = 0.05
        self.epsilon_decay = 0.992

        self.q_table: Dict[str, Dict[str, float]] = {}
        self.total_kills = 0
        self.total_damage_dealt = 0.0
        self.total_damage_taken = 0.0
        self.combat_durations: List[float] = []

        self._load_memory()

    def _state_key(self, hp: float, mp: float, target_hp: Optional[float], in_melee: bool) -> str:
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
        return f"{tgt_band}_{mp_band}_{hp_band}_{dist_band}"

    def choose_action(self, state: str, mp: float, is_stuck: bool = False) -> str:
        if state not in self.q_table:
            self.q_table[state] = {a: 0.0 for a in ACTIONS}

        # Backwards compatibility: ensure newly added actions exist in state
        for a in ACTIONS:
            if a not in self.q_table[state]:
                self.q_table[state][a] = 0.0

        if is_stuck:
            return random.choice(["JUMP_UNSTUCK", "STRAFE_LEFT", "STRAFE_RIGHT"])

        if random.random() < self.epsilon:
            choices = list(ACTIONS)
            if mp < 20.0 and "R_HEAVY_SHOT" in choices:
                choices.remove("R_HEAVY_SHOT")
            return random.choice(choices)
        else:
            action_values = self.q_table[state]
            if mp < 20.0:
                filtered = {a: v for a, v in action_values.items() if a != "R_HEAVY_SHOT"}
                if filtered:
                    action_values = filtered
            return max(action_values, key=action_values.get)

    def learn(self, state: str, action: str, reward: float, next_state: str):
        if state not in self.q_table:
            self.q_table[state] = {a: 0.0 for a in ACTIONS}
        if next_state not in self.q_table:
            self.q_table[next_state] = {a: 0.0 for a in ACTIONS}

        for a in ACTIONS:
            if a not in self.q_table[state]:
                self.q_table[state][a] = 0.0
            if a not in self.q_table[next_state]:
                self.q_table[next_state][a] = 0.0

        old_q = self.q_table[state].get(action, 0.0)
        max_next_q = max(self.q_table[next_state].values()) if self.q_table[next_state] else 0.0
        new_q = old_q + self.alpha * (reward + self.gamma * max_next_q - old_q)
        self.q_table[state][action] = round(new_q, 4)

    def decay_exploration(self):
        if self.epsilon > self.epsilon_min:
            self.epsilon = round(self.epsilon * self.epsilon_decay, 4)

    def _load_memory(self):
        if os.path.exists(COMBAT_MEMORY_FILE):
            try:
                with open(COMBAT_MEMORY_FILE, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    self.q_table = data.get("q_table", {})
                    self.total_kills = data.get("total_kills", 0)
                    self.total_damage_dealt = data.get("total_damage_dealt", 0.0)
                    self.total_damage_taken = data.get("total_damage_taken", 0.0)
                    self.epsilon = data.get("epsilon", 0.30)
                    self.combat_durations = data.get("combat_durations", [])
            except Exception:
                pass

    def save_memory(self):
        data = {
            "agent": "AION_NEXUS_AUTONOMOUS_GAMER",
            "model": "MULTI_DOMAIN_REINFORCEMENT_POLICY_V4_AGGRESSIVE",
            "last_updated": time.strftime("%Y-%m-%d %H:%M:%S"),
            "total_kills": self.total_kills,
            "total_damage_dealt": round(self.total_damage_dealt, 1),
            "total_damage_taken": round(self.total_damage_taken, 1),
            "epsilon": self.epsilon,
            "states_learned": len(self.q_table),
            "combat_durations": self.combat_durations[-50:],
            "q_table": self.q_table
        }
        try:
            with open(COMBAT_MEMORY_FILE, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
        except Exception:
            pass


class AionNexusAutonomousGamer:
    def __init__(self, initial_mode: str = "QUEST"):
        self.geom = DynamicWindowGeometry()
        self.vision = AionVision(self.geom)
        self.motor = AionMotor(self.geom)
        self.speech = AionSpeechBridge()
        self.brain = NexusCombatBrain()
        self.hotkeys = GlobalHotkeyManager()
        self.hotkeys.mode = initial_mode
        self.running = True

        self.kg = EpisodicKnowledgeGraph() if EpisodicKnowledgeGraph else None
        self.living_book = LivingMemoryBook() if LivingMemoryBook else None

        self.last_hp = 100.0
        self.last_mp = 100.0
        self.last_target_hp = None
        self.combat_start = 0.0
        self.roam_count = 0
        self.kills = self.brain.total_kills

        # Quest & Navigation timing
        self.last_quest_click = 0.0
        self.last_npc_interact = 0.0
        self.last_status_write = 0.0
        self.last_tab_scan = 0.0
        self.last_target_change = time.time()
        self.last_observed_target_hp = None

        # Cooldown Manager for all hotbar skills
        self.skill_cooldowns = {
            KEY_SKILL_1: 1.5,
            KEY_SKILL_2: 3.0,
            KEY_SKILL_3: 5.0,
            KEY_SKILL_4: 7.5,
            KEY_SKILL_5: 9.0,
            KEY_SKILL_Q: 4.0,
            KEY_SKILL_E: 6.0,
            KEY_SKILL_R: 12.0,
        }
        self.skill_last_used = {k: 0.0 for k in self.skill_cooldowns}

    def _check_ipc_commands(self):
        if os.path.exists(CMD_FILE):
            try:
                with open(CMD_FILE, "r") as f:
                    cmd = f.read().strip()
                if cmd:
                    with open(CMD_FILE, "w") as f:
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
                    elif cmd == "MODE_QUEST":
                        self.hotkeys.mode = "QUEST"
                        self.speech.speak("Quest Mode activated!", force=True)
                    elif cmd == "MODE_FARM":
                        self.hotkeys.mode = "FARM"
                        self.speech.speak("Farm Mode activated!", force=True)
                    elif cmd == "TOGGLE_MODE":
                        self.hotkeys.mode = "FARM" if self.hotkeys.mode == "QUEST" else "QUEST"
                        self.speech.speak(f"Mode switched to {self.hotkeys.mode}!", force=True)
                    elif cmd == "UNSTUCK":
                        self.speech.speak("Unstuck initiated! Evading collision.", force=True)
                        self.motor.jump_shot_unstuck()
                    elif cmd == "TARGET":
                        self.motor.press_key(KEY_TARGET, delay=60)
                    elif cmd == "CLICK_QUEST":
                        self.motor.click_quest_tracker()
                    elif cmd == "INTERACT_NPC":
                        self.motor.interact_npc()
                    elif cmd == "STOP":
                        self.running = False
            except Exception:
                pass

    def _write_status_telemetry(self, hp: float, mp: float, target_hp: Optional[float], current_action: str, reward: float):
        now = time.time()
        if (now - self.last_status_write) < 0.25:
            return
        self.last_status_write = now

        avg_ttk = sum(self.brain.combat_durations[-10:]) / max(1, len(self.brain.combat_durations[-10:]))
        payload = {
            "running": self.running,
            "paused": self.hotkeys.paused,
            "mode": self.hotkeys.mode,
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
            "timestamp": time.time()
        }
        try:
            with open(STATUS_FILE, "w") as f:
                json.dump(payload, f, indent=2)
        except Exception:
            pass

    def execute_action(self, action: str):
        """Executes offensive tactical action — EVERY action maintains damage output!"""
        now = time.time()
        if action == "L_AUTO_SHOT":
            # Rapid 3-arrow barrage
            self.motor.rapid_shoot_left(count=3, delay=0.06)
        elif action == "R_HEAVY_SHOT":
            self.motor.shoot_right()
            self.motor.rapid_shoot_left(count=1, delay=0.04)
        elif action == "SKILL_1":
            if (now - self.skill_last_used[KEY_SKILL_1]) >= self.skill_cooldowns[KEY_SKILL_1]:
                self.motor.press_key(KEY_SKILL_1, delay=65)
                self.skill_last_used[KEY_SKILL_1] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "SKILL_2":
            if (now - self.skill_last_used[KEY_SKILL_2]) >= self.skill_cooldowns[KEY_SKILL_2]:
                self.motor.press_key(KEY_SKILL_2, delay=65)
                self.skill_last_used[KEY_SKILL_2] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "SKILL_3":
            if (now - self.skill_last_used[KEY_SKILL_3]) >= self.skill_cooldowns[KEY_SKILL_3]:
                self.motor.press_key(KEY_SKILL_3, delay=65)
                self.skill_last_used[KEY_SKILL_3] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "SKILL_4":
            if (now - self.skill_last_used[KEY_SKILL_4]) >= self.skill_cooldowns[KEY_SKILL_4]:
                self.motor.press_key(KEY_SKILL_4, delay=65)
                self.skill_last_used[KEY_SKILL_4] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "SKILL_5":
            if (now - self.skill_last_used[KEY_SKILL_5]) >= self.skill_cooldowns[KEY_SKILL_5]:
                self.motor.press_key(KEY_SKILL_5, delay=65)
                self.skill_last_used[KEY_SKILL_5] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "SKILL_Q":
            if (now - self.skill_last_used[KEY_SKILL_Q]) >= self.skill_cooldowns[KEY_SKILL_Q]:
                self.motor.press_key(KEY_SKILL_Q, delay=65)
                self.skill_last_used[KEY_SKILL_Q] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "SKILL_E":
            if (now - self.skill_last_used[KEY_SKILL_E]) >= self.skill_cooldowns[KEY_SKILL_E]:
                self.motor.press_key(KEY_SKILL_E, delay=65)
                self.skill_last_used[KEY_SKILL_E] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "SKILL_R":
            if (now - self.skill_last_used[KEY_SKILL_R]) >= self.skill_cooldowns[KEY_SKILL_R]:
                self.motor.press_key(KEY_SKILL_R, delay=65)
                self.skill_last_used[KEY_SKILL_R] = now
            self.motor.rapid_shoot_left(count=2, delay=0.05)
        elif action == "TACTICAL_KITE":
            self.motor.kite_back_and_shoot(duration=0.32)
        elif action == "STRAFE_LEFT":
            self.motor.strafe_left_and_shoot(duration=0.30)
        elif action == "STRAFE_RIGHT":
            self.motor.strafe_right_and_shoot(duration=0.30)
        elif action == "JUMP_UNSTUCK":
            self.motor.jump_shot_unstuck()

    def calculate_reward(self, prev_hp: float, curr_hp: float,
                         prev_mp: float, curr_mp: float,
                         prev_tgt: Optional[float], curr_tgt: Optional[float],
                         action: str, is_stuck: bool = False) -> float:
        reward = 0.0

        # 1. Damage Dealt
        if prev_tgt is not None and curr_tgt is not None:
            dmg_dealt = prev_tgt - curr_tgt
            if dmg_dealt > 0:
                reward += dmg_dealt * 0.4
                self.brain.total_damage_dealt += dmg_dealt

        # 2. Defeat Mob
        if prev_tgt is not None and curr_tgt is None:
            reward += 25.0
            if self.combat_start > 0:
                duration = time.time() - self.combat_start
                if duration < 8.0:
                    reward += 10.0

        # 3. Damage Taken
        dmg_taken = prev_hp - curr_hp
        if dmg_taken > 0:
            reward -= dmg_taken * 0.6
            self.brain.total_damage_taken += dmg_taken
            if action in ["TACTICAL_KITE", "STRAFE_LEFT", "STRAFE_RIGHT"]:
                reward += 1.5

        # 4. Mana penalty
        if curr_mp < 15.0 and action == "R_HEAVY_SHOT":
            reward -= 3.0

        # 5. Kiting & Strafing Survival Bonus
        if curr_hp < 55.0 and action in ["TACTICAL_KITE", "STRAFE_LEFT", "STRAFE_RIGHT"]:
            reward += 3.0

        # 6. Stuck Penalties vs Unstuck Rewards
        if is_stuck:
            if action in ["JUMP_UNSTUCK", "STRAFE_LEFT", "STRAFE_RIGHT"]:
                reward += 4.0
            else:
                reward -= 3.5

        return round(reward, 3)

    def record_evolution_milestone(self, kills: int, avg_ttk: float):
        if self.kg:
            try:
                node_id = f"aion2_mastery_kill_{kills}"
                self.kg.nodes[node_id] = {
                    "type": "EVOLUTIONARY_MILESTONE",
                    "label": f"AION 2 Autonomous Combat Experience: {kills} Kills",
                    "avg_ttk": round(avg_ttk, 2),
                    "exploration_rate": self.brain.epsilon,
                    "states_known": len(self.brain.q_table),
                    "timestamp": time.time()
                }
                self.kg.edges.append({
                    "source": "tobias",
                    "target": node_id,
                    "relation": "MENTORED_GAMING_BREAKTHROUGH",
                    "weight": 1.0
                })
                with open(os.path.join(AI_DIR, "aion_episodic_graph.json"), "w", encoding="utf-8") as f:
                    json.dump({"nodes": self.kg.nodes, "edges": self.kg.edges}, f, indent=2, ensure_ascii=False)
            except Exception:
                pass

    def run(self):
        self.motor.focus_game()
        time.sleep(0.3)

        msg = f"Aion Nexus autonomous learning & questing active! Mode: {self.hotkeys.mode}."
        self.speech.speak(msg, force=True)
        AionLogger.log(f"=== AION NEXUS SESSION STARTED (Mode: {self.hotkeys.mode}) ===")

        print("\n" + "="*80)
        print("  🧠 AION NEXUS — SOVEREIGN MULTI-DOMAIN AUTONOMOUS AGENT (AION 2)")
        print(f"  • Mode: {self.hotkeys.mode}  (Press [F8] to toggle: QUEST <-> FARM)")
        print(f"  • GUI & Live Logs: Run 'aion2-gaming-gui' for control window!")
        print(f"  • Kills so far: {self.brain.total_kills} | Q-States: {len(self.brain.q_table)}")
        print("  🎮 PAUSE: Press [F10] or [Pause] in-game at any time for manual takeover!")
        print("="*80 + "\n")

        current_action = "L_AUTO_SHOT"
        reward = 0.0

        while self.running:
            try:
                # 1. IPC & Hotkey Checks
                self._check_ipc_commands()

                if self.hotkeys.check_pause_toggle():
                    if self.hotkeys.paused:
                        self.motor.stop_movement()
                        self.speech.speak("Aion paused! Manual control engaged.", force=True)
                        AionLogger.log("[PAUSE] Aion paused — manual control activated.")
                    else:
                        self.speech.speak(f"Aion resuming! Mode: {self.hotkeys.mode}", force=True)
                        AionLogger.log(f"[RESUME] Aion resuming (Mode: {self.hotkeys.mode}).")

                if self.hotkeys.check_mode_toggle():
                    self.speech.speak(f"Mode switched to {self.hotkeys.mode}!", force=True)
                    AionLogger.log(f"[MODE TOGGLE] New Mode: {self.hotkeys.mode}")

                # 2. Vision Perception
                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # Write live telemetry for GUI
                self._write_status_telemetry(hp, mp, target_hp, current_action, reward)

                if self.hotkeys.paused:
                    sys.stdout.write(f"\r  ⏸️  [PAUSE MODE] Manual Control | HP: {hp:5.1f}% | MP: {mp:5.1f}% | [F10/Pause to Resume]   ")
                    sys.stdout.flush()
                    time.sleep(0.12)
                    continue

                self.motor.focus_game()

                # Emergency Potions
                if 0.0 < hp < 40.0:
                    self.motor.press_key(KEY_POTION_HP, delay=80)
                    time.sleep(0.12)
                if 0.0 < mp < 20.0:
                    self.motor.press_key(KEY_POTION_MP, delay=80)
                    time.sleep(0.12)

                # --- TARGET COMBAT VS QUEST / ROAM ---
                if target_hp is None:
                    # Mob just defeated!
                    if self.combat_start > 0:
                        duration = time.time() - self.combat_start
                        self.kills += 1
                        self.brain.total_kills = self.kills
                        self.brain.combat_durations.append(round(duration, 2))
                        self.brain.decay_exploration()

                        reward = self.calculate_reward(self.last_hp, hp, self.last_mp, mp, self.last_target_hp, None, current_action)
                        prev_state = self.brain._state_key(self.last_hp, self.last_mp, self.last_target_hp, False)
                        next_state = self.brain._state_key(hp, mp, None, False)
                        self.brain.learn(prev_state, current_action, reward, next_state)
                        self.brain.save_memory()

                        avg_ttk = sum(self.brain.combat_durations[-10:]) / max(1, len(self.brain.combat_durations[-10:]))
                        log_msg = f"⭐ Kill #{self.kills} defeated in {duration:.1f}s (Avg TTK: {avg_ttk:.1f}s) | Q-Update: +{reward:.1f}"
                        print(f"\n[AION NEXUS] {log_msg}")
                        AionLogger.log(log_msg)

                        if self.kills % 3 == 0:
                            self.speech.speak(f"Kill number {self.kills}! Average TTK: {avg_ttk:.1f} seconds.")
                            self.record_evolution_milestone(self.kills, avg_ttk)

                        # Quick loot
                        self.motor.walk_forward(duration=0.6)
                        self.motor.loot()
                        self.motor.step_back(duration=0.3)

                        self.combat_start = 0.0
                        self.last_target_hp = None

                        # IMMEDIATE CHAIN-TARGET ACQUISITION: Check next pack mob!
                        self.motor.press_key(KEY_TARGET, delay=60)
                        time.sleep(0.15)
                        chain_target = self.vision.get_target_hp()
                        if chain_target is not None:
                            AionLogger.log(f"[CHAIN TARGET] Instantly locked next mob! HP: {chain_target:.1f}%")
                            target_hp = chain_target
                            continue

                        if self.hotkeys.mode == "QUEST":
                            time.sleep(0.2)
                            self.motor.click_quest_tracker()
                            self.last_quest_click = time.time()

                    # NO ACTIVE TARGET: PROACTIVE SCANNING
                    now = time.time()

                    if self.hotkeys.mode == "QUEST":
                        # 1. Proactive Tab Scan for Quest Mobs!
                        if (now - self.last_tab_scan) > 0.35:
                            self.motor.press_key(KEY_TARGET, delay=60)
                            self.last_tab_scan = now
                            time.sleep(0.12)
                            scanned_target = self.vision.get_target_hp()
                            if scanned_target is not None:
                                AionLogger.log(f"[QUEST TARGET FOUND] Mob locked via Tab! HP: {scanned_target:.1f}%")
                                target_hp = scanned_target
                                continue

                        # 2. Auto-Pathing Click (keep character moving towards objective)
                        if (now - self.last_quest_click) > 5.0:
                            msg = "[QUEST] 📜 Clicking Quest Tracker for auto-pathing..."
                            sys.stdout.write(f"\r{msg}                                  ")
                            sys.stdout.flush()
                            AionLogger.log("[QUEST] Auto-Pathing click on Quest Tracker")
                            self.motor.click_quest_tracker()
                            self.last_quest_click = now

                        # 3. NPC Dialogue / Interaction Advance
                        if (now - self.last_npc_interact) > 2.5:
                            msg = "[QUEST] 💬 Checking NPC Interaction / Dialogue ([F] / [Space])..."
                            sys.stdout.write(f"\r{msg}                                  ")
                            sys.stdout.flush()
                            self.motor.interact_npc()
                            self.last_npc_interact = now

                        time.sleep(0.15)

                    else:
                        # FARM SCOUTING: Proactive Tab Scan
                        sys.stdout.write(f"\r[FARM SCOUT] Scanning target with [Tab]... (Try {self.roam_count+1}) [Kills: {self.kills}]   ")
                        sys.stdout.flush()
                        self.motor.press_key(KEY_TARGET, delay=60)
                        time.sleep(0.15)
                        scanned_target = self.vision.get_target_hp()
                        if scanned_target is not None:
                            AionLogger.log(f"[FARM TARGET FOUND] Mob locked via Tab! HP: {scanned_target:.1f}%")
                            target_hp = scanned_target
                            continue

                        self.roam_count += 1
                        if self.roam_count >= 3:
                            # Natural roaming with slight angle turns
                            side = random.choice([KEY_LEFT, KEY_RIGHT])
                            subprocess.run(["xdotool", "key", side], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                            self.motor.walk_forward(duration=0.5)
                            self.roam_count = 0
                            time.sleep(0.15)

                else:
                    # ACTIVE ENEMY LOCKED IN SIGHT!
                    self.roam_count = 0
                    now = time.time()
                    if self.combat_start == 0.0:
                        self.combat_start = now
                        self.speech.speak("Target locked! Engaging combat & tactical kiting.")
                        AionLogger.log(f"[COMBAT START] Target locked! Initial HP: {target_hp:.1f}%")

                    # Stuck detection: target HP not changing for > 7s
                    if self.last_observed_target_hp is None or abs(target_hp - self.last_observed_target_hp) > 2.0:
                        self.last_target_change = now
                        self.last_observed_target_hp = target_hp

                    is_stuck = (now - self.last_target_change) > 7.5

                    in_melee = (now - self.combat_start) > 2.2 or (self.last_hp - hp) > 3.0
                    state = self.brain._state_key(hp, mp, target_hp, in_melee)
                    current_action = self.brain.choose_action(state, mp, is_stuck=is_stuck)

                    # Execute chosen offensive action
                    self.execute_action(current_action)

                    # Continuous baseline attack: if MP > 20%, also weave in heavy shot occasionally
                    if mp > 25.0 and random.random() < 0.25:
                        self.motor.shoot_right()

                    # Observe consequence
                    time.sleep(0.12)
                    new_hp = self.vision.get_player_hp()
                    new_mp = self.vision.get_player_mp()
                    new_target = self.vision.get_target_hp()

                    reward = self.calculate_reward(hp, new_hp, mp, new_mp, target_hp, new_target, current_action, is_stuck=is_stuck)
                    new_in_melee = (time.time() - self.combat_start) > 2.2 or (hp - new_hp) > 3.0
                    next_state = self.brain._state_key(new_hp, new_mp, new_target, new_in_melee)

                    # Q-Update
                    self.brain.learn(state, current_action, reward, next_state)

                    # Telemetry
                    q_val = self.brain.q_table.get(state, {}).get(current_action, 0.0)
                    mode_str = f"[{self.hotkeys.mode}]"
                    stuck_str = " ⚠️[STUCK]" if is_stuck else ""
                    status = (f"\r{mode_str} Target: {target_hp:4.1f}% | Action: {current_action:14s}{stuck_str} | "
                              f"Q: {q_val:5.2f} (R: {reward:+4.1f}) | Kills: {self.kills} | HP: {hp:4.0f}% MP: {mp:4.0f}%")
                    sys.stdout.write(status)
                    sys.stdout.flush()

                    self.last_hp = new_hp
                    self.last_mp = new_mp
                    self.last_target_hp = new_target

            except KeyboardInterrupt:
                break
            except Exception:
                time.sleep(0.2)

        self.brain.save_memory()
        self.speech.speak(f"Aion Nexus finished. {self.kills} Kills reinforced in neural policy!", force=True)
        AionLogger.log(f"=== AION NEXUS SESSION FINISHED ({self.kills} Kills) ===")
        print("\n\n[Aion Nexus] Finished. Memory secured.\n")


if __name__ == "__main__":
    mode = "QUEST"
    args = [a.lower().lstrip("-") for a in sys.argv[1:]]
    if "farm" in args or "autofarm" in args:
        mode = "FARM"
    elif "quest" in args:
        mode = "QUEST"

    gamer = AionNexusAutonomousGamer(initial_mode=mode)
    gamer.run()
