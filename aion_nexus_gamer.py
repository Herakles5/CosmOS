#!/usr/bin/env python3
# ==============================================================================
# AION NEXUS — AUTONOMOUS REINFORCEMENT LEARNING GAME AGENT (AION 2)
# ==============================================================================
# Sovereign AI agent that autonomously plays AION 2, farms, quests, and learns:
#   1. Perception: Dynamic window-geometry-aware HUD computer vision
#      (Supports ANY window size: 1280x720, 1920x1080, Fullscreen or Tiled).
#   2. Reinforcement Learning (Q-Learning):
#      - Balances continuous L-Fire, Mana-throttled R-Fire, Skills 1-4 & Kiting.
#      - Evaluates reward signals (+DPS, +Kills, +KitingDistance, -Damage, -OOM).
#      - Persists policies in 'aion_nexus_combat_memory.json'.
#      - Syncs milestones to Aion's Episodic Knowledge Graph & Living Book.
#   3. Modes:
#      - [F8] Toggle: AUTO-FARM (Area Mob Hunting) <---> QUEST MODE (Auto-Pathing & NPCs)
#      - [F10] / [Pause]: Global PAUSE / RESUME (Yield full control to Tobias)
#   4. Voice Bridge: Aion reflects on learning progress via MeinOS TTS.
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

# Normalized HUD Ratios (Resolution Independent)
# Player HP: x: 38.4%..48.1%, y: 90.3%
NORM_HP_X_START = 0.3844
NORM_HP_X_END = 0.4813
NORM_HP_Y = 0.9028

# Player MP: x: 51.8%..61.3%, y: 90.6%
NORM_MP_X_START = 0.5182
NORM_MP_X_END = 0.6130
NORM_MP_Y = 0.9055

# Enemy Target Bar: x: 42.3%..67.6%, y: 4.9%
NORM_TGT_X_START = 0.4234
NORM_TGT_X_END = 0.6755
NORM_TGT_Y = 0.0491

# Quest Tracker Entry (Click to Auto-Run): x: 92.2%, y: 27.1%
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

# Actions in Q-Space
ACTIONS = [
    "L_AUTO_SHOT",    # 0: Left mouse button continuous
    "R_HEAVY_SHOT",   # 1: Right mouse button heavy shot
    "SKILL_1",        # 2: Bow Skill 1
    "SKILL_3",        # 3: Bow Skill 3 (Rapid Fire)
    "SKILL_2",        # 4: Bow Skill 2 (Piercing)
    "SKILL_4",        # 5: Bow Skill 4 (Stun / Heavy)
    "TACTICAL_KITE",  # 6: Step back with [S] to maintain distance
]


class AionSpeechBridge:
    def __init__(self):
        self.bridge_pipe = "/tmp/ide_to_aion.txt"
        self.last_spoken = 0.0
        self.cooldown = 4.0

    def speak(self, text: str, force: bool = False):
        now = time.time()
        if not force and (now - self.last_spoken) < self.cooldown:
            return
        self.last_spoken = now
        formatted = f"[AION] {text}\n"
        print(f"\n\033[95m{formatted.strip()}\033[0m")
        try:
            with open(self.bridge_pipe, "a") as f:
                f.write(formatted)
        except Exception:
            pass


class DynamicWindowGeometry:
    """Tracks game window position and dimensions dynamically."""
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
    """Reads game HUD with resolution-independent dynamic window geometry."""
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
            # Sample across rows [-1, 0, 1, 2] to account for any scaling jitter
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
    """Sends mouse and keyboard inputs to the game window."""
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

    def walk_forward(self, duration: float = 0.6):
        try:
            subprocess.run(["xdotool", "keydown", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def stop_movement(self):
        try:
            subprocess.run(["xdotool", "keyup", KEY_FORWARD, "keyup", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def loot(self):
        for _ in range(3):
            self.press_key(KEY_LOOT, delay=80)
            time.sleep(0.22)

    def click_quest_tracker(self):
        """Clicks the quest tracker entry in HUD to trigger auto-pathfinding."""
        if not self.geom.wid:
            self.geom.update(force=True)
        if not self.geom.wid:
            return
        qx = int(NORM_QUEST_X * self.geom.w)
        qy = int(NORM_QUEST_Y * self.geom.h)
        cx = int(self.geom.w * 0.5)
        cy = int(self.geom.h * 0.5)
        try:
            # Click quest tracker
            subprocess.run(["xdotool", "mousemove", "--window", str(self.geom.wid), str(qx), str(qy), "click", "1"],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.08)
            # Return cursor to center to avoid camera twitching
            subprocess.run(["xdotool", "mousemove", "--window", str(self.geom.wid), str(cx), str(cy)],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def interact_npc(self):
        """Talks to NPC and advances dialogue / accepts quest."""
        self.press_key(KEY_INTERACT, delay=90)
        time.sleep(0.2)
        self.press_key(KEY_DIALOGUE_ADVANCE, delay=70)


class GlobalHotkeyManager:
    """
    Non-blocking global X11 hotkey detector:
      [F10] (76) / [Pause] (127) / [F9] (75) -> Global Pause / Resume
      [F8] (74)                              -> Toggle Auto-Farm <---> Quest Mode
    """
    def __init__(self):
        self.disp = display.Display(":0")
        self.pause_keycodes = [76, 127, 75]
        self.mode_keycodes = [74]  # F8
        self.prev_pause_down = False
        self.prev_mode_down = False
        self.paused = False
        self.mode = "FARM"  # or "QUEST"

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
                self.mode = "QUEST" if self.mode == "FARM" else "FARM"
                self.prev_mode_down = True
                return True
            elif not is_down:
                self.prev_mode_down = False
        except Exception:
            pass
        return False


class NexusCombatBrain:
    """
    Q-Learning Reinforcement Learning Model for Autonomous AION 2 Combat.
    """
    def __init__(self):
        self.alpha = 0.15     # Learning rate
        self.gamma = 0.85     # Discount factor
        self.epsilon = 0.35   # Exploration rate
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

    def choose_action(self, state: str, mp: float) -> str:
        if state not in self.q_table:
            self.q_table[state] = {a: 0.0 for a in ACTIONS}

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

        old_q = self.q_table[state][action]
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
                    self.epsilon = data.get("epsilon", 0.35)
                    self.combat_durations = data.get("combat_durations", [])
            except Exception:
                pass

    def save_memory(self):
        data = {
            "agent": "AION_NEXUS_AUTONOMOUS_GAMER",
            "model": "Q_LEARNING_REINFORCEMENT_POLICY_V2",
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
    """
    Main Sovereign Autonomous Agent:
      - Dynamic Window-Aware HUD Computer Vision
      - Autonomous Reinforcement Learning Combat & Kiting
      - Auto-Farm & Quest Pathfinding / NPC Interactions
      - Hotkeys: [F8] Mode Toggle, [F10] Global Pause
    """
    def __init__(self, initial_mode: str = "FARM"):
        self.geom = DynamicWindowGeometry()
        self.vision = AionVision(self.geom)
        self.motor = AionMotor(self.geom)
        self.speech = AionSpeechBridge()
        self.brain = NexusCombatBrain()
        self.hotkeys = GlobalHotkeyManager()
        self.hotkeys.mode = initial_mode
        self.running = True

        # Episodic Graph Link
        self.kg = EpisodicKnowledgeGraph() if EpisodicKnowledgeGraph else None
        self.living_book = LivingMemoryBook() if LivingMemoryBook else None

        self.last_hp = 100.0
        self.last_mp = 100.0
        self.last_target_hp = None
        self.combat_start = 0.0
        self.roam_count = 0
        self.kills = self.brain.total_kills

        # Quest timing
        self.last_quest_click = 0.0
        self.last_npc_interact = 0.0

        # Cooldown manager for discrete skills
        self.skill_cooldowns = {
            KEY_SKILL_1: 1.5,
            KEY_SKILL_2: 3.0,
            KEY_SKILL_3: 5.0,
            KEY_SKILL_4: 7.5,
        }
        self.skill_last_used = {k: 0.0 for k in self.skill_cooldowns}

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
            self.motor.step_back(duration=0.38)

    def calculate_reward(self, prev_hp: float, curr_hp: float,
                         prev_mp: float, curr_mp: float,
                         prev_tgt: Optional[float], curr_tgt: Optional[float],
                         action: str) -> float:
        reward = 0.0
        if prev_tgt is not None and curr_tgt is not None:
            dmg_dealt = prev_tgt - curr_tgt
            if dmg_dealt > 0:
                reward += dmg_dealt * 0.4
                self.brain.total_damage_dealt += dmg_dealt

        if prev_tgt is not None and curr_tgt is None:
            reward += 25.0
            if self.combat_start > 0:
                duration = time.time() - self.combat_start
                if duration < 8.0:
                    reward += 10.0

        dmg_taken = prev_hp - curr_hp
        if dmg_taken > 0:
            reward -= dmg_taken * 0.6
            self.brain.total_damage_taken += dmg_taken
            if action == "TACTICAL_KITE":
                reward += 1.5

        if curr_mp < 15.0 and action == "R_HEAVY_SHOT":
            reward -= 3.0

        if curr_hp < 50.0 and action == "TACTICAL_KITE":
            reward += 3.0

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
        time.sleep(0.5)

        self.speech.speak(
            f"Aion Nexus autonomes Spielen gestartet! Modus: {self.hotkeys.mode}. Ich passe mich dynamisch an dein Fenster an.",
            force=True
        )

        print("\n" + "="*80)
        print("  🧠 AION NEXUS — SOVEREIGN AUTONOMOUS AGENT (AION 2)")
        print(f"  • Aktiver Modus: {self.hotkeys.mode}  ([F8] drücken zum Umschalten: FARM <-> QUEST)")
        print(f"  • Fenster erkannt: {self.geom.w}x{self.geom.h} an ({self.geom.ax}, {self.geom.ay})")
        print(f"  • Kills bisher: {self.brain.total_kills} | Q-Zustände: {len(self.brain.q_table)}")
        print("  🎮 PAUSE-HOTKEY: Drücke jederzeit [F10] oder [Pause], um Aion zu pausieren!")
        print("  Tipp: Drücke Strg+C zum Beenden.")
        print("="*80 + "\n")

        current_action = "L_AUTO_SHOT"

        while self.running:
            try:
                # 1. Hotkey Checks: Pause [F10/Pause] & Mode Toggle [F8]
                if self.hotkeys.check_pause_toggle():
                    if self.hotkeys.paused:
                        self.motor.stop_movement()
                        self.speech.speak("Aion pausiert! Du hast jetzt die volle Kontrolle, Tobias.", force=True)
                        print("\n\n" + "-"*65)
                        print("  ⏸️  [PAUSIERT] Aion pausiert — Tobias steuert jetzt selbst!")
                        print("  (Drücke jederzeit [F10] oder [Pause] im Spiel zum Fortsetzen)")
                        print("-"*65 + "\n")
                    else:
                        self.speech.speak(f"Aion übernimmt wieder! Modus: {self.hotkeys.mode}", force=True)
                        print("\n\n" + "-"*65)
                        print(f"  ▶️  [FORTGESETZT] Aion übernimmt wieder! Modus: {self.hotkeys.mode}")
                        print("-"*65 + "\n")

                if self.hotkeys.check_mode_toggle():
                    self.speech.speak(f"Modus gewechselt zu {self.hotkeys.mode}!", force=True)
                    print(f"\n\n  🔄 [MODUS-WECHSEL] Neuer Modus: {self.hotkeys.mode} (F8 zum Umschalten)\n")

                # If Paused, do nothing and wait
                if self.hotkeys.paused:
                    hp = self.vision.get_player_hp()
                    mp = self.vision.get_player_mp()
                    sys.stdout.write(f"\r  ⏸️  [PAUSE-MODUS] Tobias steuert | HP: {hp:5.1f}% | MP: {mp:5.1f}% | [F10/Pause zum Weiterspielen lassen]   ")
                    sys.stdout.flush()
                    time.sleep(0.12)
                    continue

                self.motor.focus_game()

                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # Emergency Potions
                if 0.0 < hp < 40.0:
                    self.motor.press_key(KEY_POTION_HP, delay=90)
                    time.sleep(0.15)
                if 0.0 < mp < 20.0:
                    self.motor.press_key(KEY_POTION_MP, delay=90)
                    time.sleep(0.15)

                # --- TARGET COMBAT VS EXPLORATION / QUEST ---
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
                        print(f"\n[AION NEXUS] ⭐ Kill #{self.kills} erledigt! Zeit: {duration:.1f}s (Avg TTK: {avg_ttk:.1f}s) | Q-Update: +{reward:.1f}")

                        if self.kills % 3 == 0:
                            self.speech.speak(f"Kill Nummer {self.kills}! Durchschnittliche Kampfzeit: {avg_ttk:.1f} Sekunden.")
                            self.record_evolution_milestone(self.kills, avg_ttk)

                        # Advance to corpse and collect loot
                        self.motor.walk_forward(duration=0.9)
                        self.motor.loot()
                        self.motor.step_back(duration=0.4)

                        self.combat_start = 0.0
                        self.last_target_hp = None

                        # If in Quest mode, re-trigger quest tracker after kill
                        if self.hotkeys.mode == "QUEST":
                            time.sleep(0.3)
                            self.motor.click_quest_tracker()
                            self.last_quest_click = time.time()

                    # NO ENEMY TARGETED:
                    now = time.time()

                    if self.hotkeys.mode == "QUEST":
                        # QUEST MODE LOGIC:
                        # 1. Periodically click quest tracker (every 7 seconds) to maintain auto-pathing
                        if (now - self.last_quest_click) > 7.0:
                            sys.stdout.write("\r[QUEST MODE] 📜 Klicke Quest Tracker für Auto-Lauf...                          ")
                            sys.stdout.flush()
                            self.motor.click_quest_tracker()
                            self.last_quest_click = now

                        # 2. Periodically interact with NPCs / Dialogues (every 2.5 seconds)
                        if (now - self.last_npc_interact) > 2.5:
                            sys.stdout.write("\r[QUEST MODE] 💬 Prüfe NPC-Interaktion / Dialoge ([F] / [Space])...             ")
                            sys.stdout.flush()
                            self.motor.interact_npc()
                            self.last_npc_interact = now

                        # Small pause, then re-check for target
                        time.sleep(0.18)

                    else:
                        # AUTO-FARM MODE LOGIC:
                        # Tab-Scout for next mob
                        sys.stdout.write(f"\r[FARM SCOUT] Suche Gegner mit [Tab]... (Versuch {self.roam_count+1}) [Kills: {self.kills}]   ")
                        sys.stdout.flush()
                        self.motor.press_key(KEY_TARGET, delay=70)
                        time.sleep(0.28)
                        self.roam_count += 1

                        if self.roam_count >= 3:
                            sys.stdout.write("\r[FARM ROAM] Erkunde vorwärts ([W])...                                     ")
                            sys.stdout.flush()
                            self.motor.walk_forward(duration=0.55)
                            self.roam_count = 0
                            time.sleep(0.2)

                else:
                    # TARGET DETECTED! ACTIVE COMBAT (Both Farm & Quest Modes)
                    self.roam_count = 0
                    now = time.time()
                    if self.combat_start == 0.0:
                        self.combat_start = now
                        self.speech.speak("Ziel erfasst! Eröffne Feuer und halte Distanz.")

                    in_melee = (now - self.combat_start) > 2.5 or (self.last_hp - hp) > 3.0

                    state = self.brain._state_key(hp, mp, target_hp, in_melee)
                    current_action = self.brain.choose_action(state, mp)

                    # Execute action
                    self.execute_action(current_action)

                    # Measure new state & calculate reward
                    time.sleep(0.18)
                    new_hp = self.vision.get_player_hp()
                    new_mp = self.vision.get_player_mp()
                    new_target = self.vision.get_target_hp()

                    reward = self.calculate_reward(hp, new_hp, mp, new_mp, target_hp, new_target, current_action)
                    new_in_melee = (time.time() - self.combat_start) > 2.5 or (hp - new_hp) > 3.0
                    next_state = self.brain._state_key(new_hp, new_mp, new_target, new_in_melee)

                    # Q-LEARNING UPDATE
                    self.brain.learn(state, current_action, reward, next_state)

                    # Telemetry
                    q_val = self.brain.q_table.get(state, {}).get(current_action, 0.0)
                    mode_str = f"[{self.hotkeys.mode}]"
                    status = (f"\r{mode_str} Ziel: {target_hp:4.1f}% | Action: {current_action:14s} | "
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
        self.speech.speak(f"Aion Nexus Session beendet. {self.kills} Kills im Modell gespeichert!", force=True)
        print("\n\n[Aion Nexus] Beendet. Modell gesichert.\n")


if __name__ == "__main__":
    mode = "FARM"
    args = [a.lower().lstrip("-") for a in sys.argv[1:]]
    if "quest" in args:
        mode = "QUEST"
    elif "farm" in args or "autofarm" in args:
        mode = "FARM"

    gamer = AionNexusAutonomousGamer(initial_mode=mode)
    gamer.run()
