#!/usr/bin/env python3
# ==============================================================================
# AION NEXUS — AUTONOMOUS REINFORCEMENT LEARNING GAME AGENT (AION 2)
# ==============================================================================
# Sovereign AI agent that autonomously plays AION 2 and learns from combat:
#   1. Perception: Sub-millisecond X11 HUD analysis (HP, MP, Target Bar).
#   2. Reinforcement Learning (Q-Learning):
#      - Dynamically balances L-Fire, R-Fire, Skills 1-4 & Kiting distance.
#      - Optimizes Time-To-Kill (TTK) while minimizing damage taken.
#      - Evaluates reward signals (+Damage, +Kills, -DamageTaken, -OOM penalties).
#   3. Episodic Memory Integration:
#      - Persists learned combat policies in 'aion_nexus_combat_memory.json'.
#      - Syncs breakthroughs to Aion's Episodic Knowledge Graph & Living Book.
#   4. Voice Feedback: Aion reflects on her learning curve via MeinOS Bridge.
# ==============================================================================

import os
import sys
import time
import json
import random
import subprocess
from typing import Dict, Any, Optional, Tuple
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

# --- Native 1080p Calibration ---
HP_X_START, HP_X_END, HP_Y = 738, 924, 975
HP_WIDTH = HP_X_END - HP_X_START  # 186 px

MP_X_START, MP_X_END, MP_Y = 995, 1177, 978
MP_WIDTH = MP_X_END - MP_X_START  # 182 px

TGT_X_START, TGT_X_END = 813, 1297
TGT_Y_ROWS = [52, 53, 54, 55]
TGT_WIDTH = TGT_X_END - TGT_X_START  # 484 px

# Hotkeys
KEY_TARGET = "Tab"
KEY_SKILL_1 = "1"
KEY_SKILL_2 = "2"
KEY_SKILL_3 = "3"
KEY_SKILL_4 = "4"
KEY_LOOT = "f"
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

    def speak(self, text, force=False):
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


class AionVision:
    def __init__(self):
        self.disp = display.Display(":0")
        self.root = self.disp.screen().root

    def get_player_hp(self) -> float:
        try:
            img = self.root.get_image(HP_X_START, HP_Y, HP_WIDTH, 4, X.ZPixmap, 0xffffffff)
            data = img.data
            red_count = 0
            for i in range(0, len(data), 4):
                b, g, r = data[i], data[i+1], data[i+2]
                if r > 120 and g < 70 and b < 70:
                    red_count += 1
            return min(100.0, (red_count / (HP_WIDTH * 4)) * 100.0)
        except Exception:
            return 0.0

    def get_player_mp(self) -> float:
        try:
            img = self.root.get_image(MP_X_START, MP_Y, MP_WIDTH, 3, X.ZPixmap, 0xffffffff)
            data = img.data
            blue_count = 0
            for i in range(0, len(data), 4):
                b, g, r = data[i], data[i+1], data[i+2]
                if b > 120 and r < 70:
                    blue_count += 1
            return min(100.0, (blue_count / (MP_WIDTH * 3)) * 100.0)
        except Exception:
            return 0.0

    def get_target_hp(self) -> Optional[float]:
        try:
            total_red = 0
            for y in TGT_Y_ROWS:
                img = self.root.get_image(TGT_X_START, y, TGT_WIDTH, 1, X.ZPixmap, 0xffffffff)
                data = img.data
                for i in range(0, len(data), 4):
                    b, g, r = data[i], data[i+1], data[i+2]
                    if r > 140 and g < 65 and b < 65:
                        total_red += 1
            total_sampled = TGT_WIDTH * len(TGT_Y_ROWS)
            if total_red < 35:
                return None
            pct = (total_red / total_sampled) * 100.0 * 2.5
            return max(1.0, min(100.0, pct))
        except Exception:
            return None


class AionMotor:
    def __init__(self):
        self.window_id = self.find_window()

    def find_window(self):
        try:
            out = subprocess.check_output(["xdotool", "search", "--name", AION_WINDOW_NAME], text=True).strip()
            if out:
                return out.splitlines()[0]
        except Exception:
            pass
        return None

    def focus_game(self):
        if not self.window_id:
            self.window_id = self.find_window()
        if self.window_id:
            try:
                subprocess.run(["xdotool", "windowactivate", "--sync", self.window_id], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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

    def press_key(self, key, delay=80):
        try:
            subprocess.run(["xdotool", "key", "--delay", str(delay), key], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def step_back(self, duration=0.35):
        try:
            subprocess.run(["xdotool", "keydown", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def walk_forward(self, duration=0.6):
        try:
            subprocess.run(["xdotool", "keydown", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def loot(self):
        for _ in range(3):
            self.press_key(KEY_LOOT, delay=80)
            time.sleep(0.25)


class NexusCombatBrain:
    """
    Q-Learning Reinforcement Learning Model for Autonomous AION 2 Combat.
    Continuously trains optimal combat policies and persists them.
    """
    def __init__(self):
        self.alpha = 0.15     # Learning rate
        self.gamma = 0.85     # Discount factor
        self.epsilon = 0.35   # Exploration rate (decays with mastery)
        self.epsilon_min = 0.05
        self.epsilon_decay = 0.992

        self.q_table: Dict[str, Dict[str, float]] = {}
        self.history: List[Dict[str, Any]] = []
        self.total_kills = 0
        self.total_damage_dealt = 0.0
        self.total_damage_taken = 0.0
        self.combat_durations = []

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

        # Exploration vs Exploitation
        if random.random() < self.epsilon:
            # Exploration
            choices = list(ACTIONS)
            # Discourage R_HEAVY if low mana
            if mp < 20.0 and "R_HEAVY_SHOT" in choices:
                choices.remove("R_HEAVY_SHOT")
            return random.choice(choices)
        else:
            # Exploitation (Best learned Q-action)
            action_values = self.q_table[state]
            if mp < 20.0:
                # Mask out R_HEAVY when OOM
                filtered = {a: v for a, v in action_values.items() if a != "R_HEAVY_SHOT"}
                if filtered:
                    action_values = filtered
            best_action = max(action_values, key=action_values.get)
            return best_action

    def learn(self, state: str, action: str, reward: float, next_state: str):
        if state not in self.q_table:
            self.q_table[state] = {a: 0.0 for a in ACTIONS}
        if next_state not in self.q_table:
            self.q_table[next_state] = {a: 0.0 for a in ACTIONS}

        old_q = self.q_table[state][action]
        max_next_q = max(self.q_table[next_state].values()) if self.q_table[next_state] else 0.0
        # Q(S,A) = Q(S,A) + alpha * [Reward + gamma * max Q(S',a) - Q(S,A)]
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
    Main Autonomous Agent that observes, acts, learns, and reflects.
    """
    def __init__(self):
        self.vision = AionVision()
        self.motor = AionMotor()
        self.speech = AionSpeechBridge()
        self.brain = NexusCombatBrain()
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

        # 1. Damage Dealt Reward (Target HP reduced)
        if prev_tgt is not None and curr_tgt is not None:
            dmg_dealt = prev_tgt - curr_tgt
            if dmg_dealt > 0:
                reward += dmg_dealt * 0.4
                self.brain.total_damage_dealt += dmg_dealt

        # 2. Kill Reward
        if prev_tgt is not None and curr_tgt is None:
            reward += 25.0  # Big reward for defeating mob!
            # Speed bonus if killed under 8 seconds
            if self.combat_start > 0:
                duration = time.time() - self.combat_start
                if duration < 8.0:
                    reward += 10.0

        # 3. Damage Taken Penalty
        dmg_taken = prev_hp - curr_hp
        if dmg_taken > 0:
            reward -= dmg_taken * 0.6  # Penalty for getting hit
            self.brain.total_damage_taken += dmg_taken
            # If hit while kiting, smaller penalty than standing still
            if action == "TACTICAL_KITE":
                reward += 1.5

        # 4. Mana Efficiency
        if curr_mp < 15.0 and action == "R_HEAVY_SHOT":
            reward -= 3.0  # Penalty for spamming Mana when empty

        # 5. Kiting Incentive when wounded
        if curr_hp < 50.0 and action == "TACTICAL_KITE":
            reward += 3.0  # Good survival instinct!

        return round(reward, 3)

    def record_evolution_milestone(self, kills: int, avg_ttk: float):
        """Records self-evolution progress in Aion's Living Memory Graph."""
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
                # Save graph
                with open(os.path.join(AI_DIR, "aion_episodic_graph.json"), "w", encoding="utf-8") as f:
                    json.dump({"nodes": self.kg.nodes, "edges": self.kg.edges}, f, indent=2, ensure_ascii=False)
            except Exception:
                pass

    def run(self):
        self.motor.focus_game()
        time.sleep(0.5)

        self.speech.speak(
            "Aion Nexus autonomes Lernen aktiviert! Ich spiele AION 2 jetzt eigenständig und verbessere meine Kampfmuster mit jedem Kampf.",
            force=True
        )

        print("\n" + "="*80)
        print("  🧠 AION NEXUS — AUTONOMOUS REINFORCEMENT LEARNING GAMER")
        print(f"  • Kills bisher gelernt: {self.brain.total_kills}")
        print(f"  • Bekannte Zustandskombinationen: {len(self.brain.q_table)}")
        print(f"  • Explorations-Rate: {self.brain.epsilon*100:.1f}%")
        print("  • Belohnungen: +DPS, +Kills, +Kiting-Distanz | -Schaden, -OOM")
        print("  Tipp: Drücke Strg+C zum Beenden.")
        print("="*80 + "\n")

        current_action = "L_AUTO_SHOT"

        while self.running:
            try:
                self.motor.focus_game()

                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # Emergency Potion Overrides
                if 0.0 < hp < 40.0:
                    self.motor.press_key(KEY_POTION_HP, delay=90)
                    time.sleep(0.2)
                if 0.0 < mp < 20.0:
                    self.motor.press_key(KEY_POTION_MP, delay=90)
                    time.sleep(0.2)

                # --- STATE MACHINE ---
                if target_hp is None:
                    # No target in sight
                    if self.combat_start > 0:
                        # TARGET JUST DIED!
                        duration = time.time() - self.combat_start
                        self.kills += 1
                        self.brain.total_kills = self.kills
                        self.brain.combat_durations.append(round(duration, 2))
                        self.brain.decay_exploration()

                        # Reward the final kill transition
                        reward = self.calculate_reward(self.last_hp, hp, self.last_mp, mp, self.last_target_hp, None, current_action)
                        prev_state = self.brain._state_key(self.last_hp, self.last_mp, self.last_target_hp, False)
                        next_state = self.brain._state_key(hp, mp, None, False)
                        self.brain.learn(prev_state, current_action, reward, next_state)
                        self.brain.save_memory()

                        # Voice reflection & Knowledge Graph update every 3 kills
                        avg_ttk = sum(self.brain.combat_durations[-10:]) / max(1, len(self.brain.combat_durations[-10:]))
                        print(f"\n[AION NEXUS] ⭐ Kill #{self.kills} erledigt! Zeit: {duration:.1f}s (Avg TTK: {avg_ttk:.1f}s) | Q-Update: +{reward:.1f}")

                        if self.kills % 3 == 0:
                            self.speech.speak(f"Kill Nummer {self.kills}! Mein neuronales Kampfmodell lernt: Durchschnittliche Kampfzeit liegt bei {avg_ttk:.1f} Sekunden.")
                            self.record_evolution_milestone(self.kills, avg_ttk)

                        # Advance to corpse and collect loot
                        self.motor.walk_forward(duration=1.0)
                        self.motor.loot()
                        self.motor.step_back(duration=0.4)

                        self.combat_start = 0.0
                        self.last_target_hp = None

                    # Scout / Tab for next mob
                    sys.stdout.write(f"\r[NEXUS SCOUT] Suche Gegner mit [Tab]... (Versuch {self.roam_count+1}) [Q-States: {len(self.brain.q_table)}]    ")
                    sys.stdout.flush()
                    self.motor.press_key(KEY_TARGET, delay=70)
                    time.sleep(0.3)
                    self.roam_count += 1

                    if self.roam_count >= 3:
                        sys.stdout.write("\r[NEXUS ROAM] Kein Ziel in Reichweite -> Erkunde vorwärts ([W])...                  ")
                        sys.stdout.flush()
                        self.motor.walk_forward(duration=0.55)
                        self.roam_count = 0
                        time.sleep(0.2)

                else:
                    # TARGET IN SIGHT! ACTIVE COMBAT
                    self.roam_count = 0
                    now = time.time()
                    if self.combat_start == 0.0:
                        self.combat_start = now
                        self.speech.speak("Ziel erfasst! Berechne optimale Angriffs-Trajektorie.")

                    # Detect if in melee range (taking damage or in fight > 2.5s)
                    in_melee = (now - self.combat_start) > 2.5 or (self.last_hp - hp) > 3.0

                    state = self.brain._state_key(hp, mp, target_hp, in_melee)
                    current_action = self.brain.choose_action(state, mp)

                    # Execute chosen action
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
                    status = (f"\r[NEXUS RL] Ziel: {target_hp:4.1f}% | Action: {current_action:14s} | "
                              f"Q: {q_val:5.2f} (R: {reward:+4.1f}) | Kills: {self.kills} | ε: {self.brain.epsilon*100:4.1f}%")
                    sys.stdout.write(status)
                    sys.stdout.flush()

                    self.last_hp = new_hp
                    self.last_mp = new_mp
                    self.last_target_hp = new_target

            except KeyboardInterrupt:
                break
            except Exception as e:
                time.sleep(0.3)

        self.brain.save_memory()
        self.speech.speak(f"Aion Nexus Lernsession beendet. {self.kills} Kills im Wissensgraphen verankert!", force=True)
        print("\n\n[Aion Nexus] Kampftraining beendet. Modell gespeichert.\n")


if __name__ == "__main__":
    gamer = AionNexusAutonomousGamer()
    gamer.run()
