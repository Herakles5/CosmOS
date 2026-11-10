#!/usr/bin/env python3
# ==============================================================================
# AION NEXUS — AUTONOMOUS MULTI-DOMAIN REINFORCEMENT LEARNING AGENT (AION 2)
# ==============================================================================
# Sovereign AI agent that autonomously plays AION 2, learns from:
#   1. Combat: Q-Learning optimal rotations (L-Fire, R-Fire, Skills 1-4, Mana).
#   2. Movements: Circle-kiting (Strafing A/D), Tactical backward kiting (S),
#      and Collision Unstuck (Jump + Leap).
#   3. Questing: Auto-clicking Quest Tracker, navigating to objective,
#      talking with NPCs ([F]/[Space]), accepting rewards & defending along paths.
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

# Normalized HUD Ratios (Resolution Independent)
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

# Actions in Multi-Domain Q-Space (Combat, Movement & Kiting)
ACTIONS = [
    "L_AUTO_SHOT",    # 0: Continuous primary bow shot
    "R_HEAVY_SHOT",   # 1: Heavy bow shot (mana throttled)
    "SKILL_1",        # 2: Bow Skill 1 (Primary)
    "SKILL_2",        # 3: Bow Skill 2 (Piercing)
    "SKILL_3",        # 4: Bow Skill 3 (Rapid Fire)
    "SKILL_4",        # 5: Bow Skill 4 (Stun / Burst)
    "TACTICAL_KITE",  # 6: Step backward with [S]
    "STRAFE_LEFT",    # 7: Circle-kite left with [A]
    "STRAFE_RIGHT",   # 8: Circle-kite right with [D]
    "JUMP_UNSTUCK",   # 9: Jump [Space] + forward leap to clear collision
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
                subprocess.run(["xdotool", "windowactivate", "--sync", str(self.geom.wid)],
                               check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception:
                pass

    def shoot_left(self):
        try:
            subprocess.run(["xdotool", "mousedown", "1"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.04)
            subprocess.run(["xdotool", "mouseup", "1"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def shoot_right(self):
        try:
            subprocess.run(["xdotool", "mousedown", "3"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.04)
            subprocess.run(["xdotool", "mouseup", "3"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def press_key(self, key: str, delay: int = 80):
        try:
            subprocess.run(["xdotool", "key", "--delay", str(delay), key], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def step_back(self, duration: float = 0.35):
        try:
            subprocess.run(["xdotool", "keydown", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def strafe_left(self, duration: float = 0.32):
        try:
            subprocess.run(["xdotool", "keydown", KEY_LEFT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_LEFT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def strafe_right(self, duration: float = 0.32):
        try:
            subprocess.run(["xdotool", "keydown", KEY_RIGHT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_RIGHT], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def jump_unstuck(self):
        """Performs a leap forward + jump to clear terrain collision."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run(["xdotool", "key", KEY_JUMP], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.35)
            subprocess.run(["xdotool", "keyup", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def walk_forward(self, duration: float = 0.6):
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
            self.press_key(KEY_LOOT, delay=80)
            time.sleep(0.22)

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
        self.press_key(KEY_INTERACT, delay=90)
        time.sleep(0.2)
        self.press_key(KEY_DIALOGUE_ADVANCE, delay=70)


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
    """Multi-Domain Reinforcement Learning (Combat, Circle Kiting, Unstuck, Quest)."""
    def __init__(self):
        self.alpha = 0.15
        self.gamma = 0.85
        self.epsilon = 0.34
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

        if is_stuck:
            # High priority to unstuck / strafe
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
                    self.epsilon = data.get("epsilon", 0.34)
                    self.combat_durations = data.get("combat_durations", [])
            except Exception:
                pass

    def save_memory(self):
        data = {
            "agent": "AION_NEXUS_AUTONOMOUS_GAMER",
            "model": "MULTI_DOMAIN_REINFORCEMENT_POLICY_V3",
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
        self.last_target_change = time.time()
        self.last_observed_target_hp = None

        # Cooldown manager
        self.skill_cooldowns = {
            KEY_SKILL_1: 1.5,
            KEY_SKILL_2: 3.0,
            KEY_SKILL_3: 5.0,
            KEY_SKILL_4: 7.5,
        }
        self.skill_last_used = {k: 0.0 for k in self.skill_cooldowns}

    def _check_ipc_commands(self):
        if os.path.exists(CMD_FILE):
            try:
                with open(CMD_FILE, "r") as f:
                    cmd = f.read().strip()
                if cmd:
                    # Clear command file
                    with open(CMD_FILE, "w") as f:
                        f.write("")

                    AionLogger.log(f"[IPC COMMAND RECEIVED] {cmd}")
                    if cmd == "PAUSE":
                        self.hotkeys.paused = True
                        self.motor.stop_movement()
                        self.speech.speak("Aion pausiert! Du übernimmst die Steuerung, Tobias.", force=True)
                    elif cmd == "RESUME":
                        self.hotkeys.paused = False
                        self.speech.speak(f"Aion setzt fort! Modus: {self.hotkeys.mode}", force=True)
                    elif cmd == "TOGGLE_PAUSE":
                        self.hotkeys.paused = not self.hotkeys.paused
                        if self.hotkeys.paused:
                            self.motor.stop_movement()
                            self.speech.speak("Aion pausiert! Du hast die Kontrolle.", force=True)
                        else:
                            self.speech.speak(f"Aion setzt fort! Modus: {self.hotkeys.mode}", force=True)
                    elif cmd == "MODE_QUEST":
                        self.hotkeys.mode = "QUEST"
                        self.speech.speak("Quest-Modus aktiviert!", force=True)
                    elif cmd == "MODE_FARM":
                        self.hotkeys.mode = "FARM"
                        self.speech.speak("Farm-Modus aktiviert!", force=True)
                    elif cmd == "TOGGLE_MODE":
                        self.hotkeys.mode = "FARM" if self.hotkeys.mode == "QUEST" else "QUEST"
                        self.speech.speak(f"Modus: {self.hotkeys.mode}!", force=True)
                    elif cmd == "UNSTUCK":
                        self.speech.speak("Befreie Charakter aus Geländekollision!", force=True)
                        self.motor.jump_unstuck()
                    elif cmd == "TARGET":
                        self.motor.press_key(KEY_TARGET, delay=70)
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
        now = time.time()
        if action == "L_AUTO_SHOT":
            self.motor.shoot_left()
        elif action == "R_HEAVY_SHOT":
            self.motor.shoot_right()
        elif action == "SKILL_1":
            if (now - self.skill_last_used[KEY_SKILL_1]) >= self.skill_cooldowns[KEY_SKILL_1]:
                self.motor.press_key(KEY_SKILL_1, delay=85)
                self.skill_last_used[KEY_SKILL_1] = now
            else:
                self.motor.shoot_left()
        elif action == "SKILL_2":
            if (now - self.skill_last_used[KEY_SKILL_2]) >= self.skill_cooldowns[KEY_SKILL_2]:
                self.motor.press_key(KEY_SKILL_2, delay=85)
                self.skill_last_used[KEY_SKILL_2] = now
            else:
                self.motor.shoot_left()
        elif action == "SKILL_3":
            if (now - self.skill_last_used[KEY_SKILL_3]) >= self.skill_cooldowns[KEY_SKILL_3]:
                self.motor.press_key(KEY_SKILL_3, delay=85)
                self.skill_last_used[KEY_SKILL_3] = now
            else:
                self.motor.shoot_left()
        elif action == "SKILL_4":
            if (now - self.skill_last_used[KEY_SKILL_4]) >= self.skill_cooldowns[KEY_SKILL_4]:
                self.motor.press_key(KEY_SKILL_4, delay=85)
                self.skill_last_used[KEY_SKILL_4] = now
            else:
                self.motor.shoot_left()
        elif action == "TACTICAL_KITE":
            self.motor.step_back(duration=0.35)
        elif action == "STRAFE_LEFT":
            self.motor.strafe_left(duration=0.32)
        elif action == "STRAFE_RIGHT":
            self.motor.strafe_right(duration=0.32)
        elif action == "JUMP_UNSTUCK":
            self.motor.jump_unstuck()

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
                    "label": f"AION 2 Autonome Kampferfahrung: {kills} Kills",
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
        time.sleep(0.4)

        msg = f"Aion Nexus autonomes Lernen & Questing aktiv! Modus: {self.hotkeys.mode}."
        self.speech.speak(msg, force=True)
        AionLogger.log(f"=== AION NEXUS SESSION GESTARTET (Modus: {self.hotkeys.mode}) ===")

        print("\n" + "="*80)
        print("  🧠 AION NEXUS — SOVEREIGN MULTI-DOMAIN AUTONOMOUS AGENT (AION 2)")
        print(f"  • Modus: {self.hotkeys.mode}  ([F8] drücken zum Wechseln: QUEST <-> FARM)")
        print(f"  • GUI & Live-Logs: Starte 'aion2-gaming-gui' für Steuerungsfenster!")
        print(f"  • Kills bisher: {self.brain.total_kills} | Q-Zustände: {len(self.brain.q_table)}")
        print("  🎮 PAUSE: Drücke jederzeit [F10] oder [Pause] im Spiel zum Pausieren!")
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
                        self.speech.speak("Aion pausiert! Tobias steuert jetzt selbst.", force=True)
                        AionLogger.log("[PAUSE] Aion pausiert — manuelle Steuerung aktiviert.")
                    else:
                        self.speech.speak(f"Aion übernimmt wieder! Modus: {self.hotkeys.mode}", force=True)
                        AionLogger.log(f"[RESUME] Aion übernimmt wieder (Modus: {self.hotkeys.mode}).")

                if self.hotkeys.check_mode_toggle():
                    self.speech.speak(f"Modus gewechselt zu {self.hotkeys.mode}!", force=True)
                    AionLogger.log(f"[MODE TOGGLE] Neuer Modus: {self.hotkeys.mode}")

                # 2. Vision Perception
                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # Write live telemetry for GUI
                self._write_status_telemetry(hp, mp, target_hp, current_action, reward)

                if self.hotkeys.paused:
                    sys.stdout.write(f"\r  ⏸️  [PAUSE-MODUS] Tobias steuert | HP: {hp:5.1f}% | MP: {mp:5.1f}% | [F10/Pause zum Weiterspielen]   ")
                    sys.stdout.flush()
                    time.sleep(0.12)
                    continue

                self.motor.focus_game()

                # Emergency Potions
                if 0.0 < hp < 40.0:
                    self.motor.press_key(KEY_POTION_HP, delay=90)
                    time.sleep(0.15)
                if 0.0 < mp < 20.0:
                    self.motor.press_key(KEY_POTION_MP, delay=90)
                    time.sleep(0.15)

                # --- TARGET COMBAT VS QUEST / ROAM ---
                if target_hp is None:
                    # Target just died!
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
                        log_msg = f"⭐ Kill #{self.kills} besiegt in {duration:.1f}s (Avg TTK: {avg_ttk:.1f}s) | Q-Update: +{reward:.1f}"
                        print(f"\n[AION NEXUS] {log_msg}")
                        AionLogger.log(log_msg)

                        if self.kills % 3 == 0:
                            self.speech.speak(f"Kill Nummer {self.kills}! Durchschnittliche Kampfzeit: {avg_ttk:.1f} Sekunden.")
                            self.record_evolution_milestone(self.kills, avg_ttk)

                        # Loot corpse
                        self.motor.walk_forward(duration=0.85)
                        self.motor.loot()
                        self.motor.step_back(duration=0.4)

                        self.combat_start = 0.0
                        self.last_target_hp = None

                        if self.hotkeys.mode == "QUEST":
                            time.sleep(0.3)
                            self.motor.click_quest_tracker()
                            self.last_quest_click = time.time()

                    # NO TARGET: QUEST OR FARM LOGIC
                    now = time.time()

                    if self.hotkeys.mode == "QUEST":
                        # Auto-Pathing Click
                        if (now - self.last_quest_click) > 7.0:
                            msg = "[QUEST] 📜 Klicke Quest Tracker für Auto-Lauf..."
                            sys.stdout.write(f"\r{msg}                                  ")
                            sys.stdout.flush()
                            AionLogger.log("[QUEST] Auto-Pathing Klick auf Quest-Tracker")
                            self.motor.click_quest_tracker()
                            self.last_quest_click = now

                        # NPC Dialogue Advance
                        if (now - self.last_npc_interact) > 2.5:
                            msg = "[QUEST] 💬 Prüfe NPC-Interaktion / Dialog ([F] / [Space])..."
                            sys.stdout.write(f"\r{msg}                                  ")
                            sys.stdout.flush()
                            self.motor.interact_npc()
                            self.last_npc_interact = now

                        time.sleep(0.18)

                    else:
                        # FARM SCOUTING: Tab target
                        sys.stdout.write(f"\r[FARM SCOUT] Suche Gegner mit [Tab]... (Versuch {self.roam_count+1}) [Kills: {self.kills}]   ")
                        sys.stdout.flush()
                        self.motor.press_key(KEY_TARGET, delay=70)
                        time.sleep(0.28)
                        self.roam_count += 1

                        if self.roam_count >= 3:
                            # Natural roaming with slight angle turns
                            side = random.choice([KEY_LEFT, KEY_RIGHT])
                            subprocess.run(["xdotool", "key", side], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                            self.motor.walk_forward(duration=0.55)
                            self.roam_count = 0
                            time.sleep(0.2)

                else:
                    # ACTIVE TARGET IN SIGHT!
                    self.roam_count = 0
                    now = time.time()
                    if self.combat_start == 0.0:
                        self.combat_start = now
                        self.speech.speak("Ziel erfasst! Kampfeinsatz & Kiting gestartet.")
                        AionLogger.log(f"[COMBAT START] Ziel anvisiert! Start-HP: {target_hp:.1f}%")

                    # Stuck detection: target HP not changing for > 7s
                    if self.last_observed_target_hp is None or abs(target_hp - self.last_observed_target_hp) > 2.0:
                        self.last_target_change = now
                        self.last_observed_target_hp = target_hp

                    is_stuck = (now - self.last_target_change) > 7.5

                    in_melee = (now - self.combat_start) > 2.2 or (self.last_hp - hp) > 3.0
                    state = self.brain._state_key(hp, mp, target_hp, in_melee)
                    current_action = self.brain.choose_action(state, mp, is_stuck=is_stuck)

                    # Execute chosen action
                    self.execute_action(current_action)

                    # Observe consequence
                    time.sleep(0.18)
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
                    status = (f"\r{mode_str} Ziel: {target_hp:4.1f}% | Action: {current_action:13s}{stuck_str} | "
                              f"Q: {q_val:5.2f} (R: {reward:+4.1f}) | Kills: {self.kills} | HP: {hp:4.0f}% MP: {mp:4.0f}%")
                    sys.stdout.write(status)
                    sys.stdout.flush()

                    self.last_hp = new_hp
                    self.last_mp = new_mp
                    self.last_target_hp = new_target

            except KeyboardInterrupt:
                break
            except Exception:
                time.sleep(0.25)

        self.brain.save_memory()
        self.speech.speak(f"Aion Nexus beendet. {self.kills} Kills im neuronalen Modell verankert!", force=True)
        AionLogger.log(f"=== AION NEXUS SESSION BEENDET ({self.kills} Kills) ===")
        print("\n\n[Aion Nexus] Beendet. Gedächtnis gesichert.\n")


if __name__ == "__main__":
    mode = "QUEST"
    args = [a.lower().lstrip("-") for a in sys.argv[1:]]
    if "farm" in args or "autofarm" in args:
        mode = "FARM"
    elif "quest" in args:
        mode = "QUEST"

    gamer = AionNexusAutonomousGamer(initial_mode=mode)
    gamer.run()
