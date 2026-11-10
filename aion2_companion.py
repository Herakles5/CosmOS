#!/usr/bin/env python3
# ==============================================================================
# AION 2 AUTONOMOUS COMPANION & AUTO-FARM BOT (MeinOS AI System v2.1)
# ==============================================================================
# Tactical Ranger Combat, Kiting & Quest System:
#   1. Perception: Dynamic window-geometry-aware HUD computer vision
#      (Supports ANY window size: 1280x720, 1920x1080, Fullscreen or Tiled).
#   2. Tactical Ranger Combat:
#      - Left Mouse Button: Continuous primary bow fire (Dauerfeuer)
#      - Right Mouse Button: Heavy shot continuous fire until MP < 20%
#      - Skills 1, 2, 3, 4: Intelligently rotated with individual cooldowns
#      - Tactical Kiting: STAYS AT DISTANCE! Kites backwards with [S]
#      - Auto-Loot: Walks forward to corpse and presses [F] x3 after kill
#      - Auto-Potions: Emergency HP [7] & MP [8] management
#   3. Quest Mode:
#      - Clicks Quest Tracker (top-right under minimap) to auto-pathfind
#      - Interacts with NPCs and advances dialogue with [F] and [Space]
#      - Seamlessly defends if mobs attack, then resumes questing
#   4. Global Hotkeys:
#      - [F8]: Toggle AUTO-FARM <---> QUEST MODE
#      - [F10] / [Pause]: Global PAUSE / RESUME (Yield full control to Tobias)
# ==============================================================================

import os
import sys
import time
import subprocess
import re
from Xlib import display, X

AION_WINDOW_NAME = "AION2"

# Normalized HUD Ratios
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

# Hotkeys
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


class DynamicWindowGeometry:
    def __init__(self, win_name=AION_WINDOW_NAME):
        self.win_name = win_name
        self.wid = None
        self.ax = 0
        self.ay = 0
        self.w = 1920
        self.h = 1080
        self.last_check = 0.0
        self.update()

    def update(self, force=False):
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

    def get_player_hp(self):
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

    def get_player_mp(self):
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

    def get_target_hp(self):
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
                if active and int(active) == int(self.geom.wid):
                    return
                subprocess.run(["xdotool", "windowactivate", str(self.geom.wid)],
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
        self.pause_keycodes = [76, 127, 75]
        self.mode_keycodes = [74]  # F8
        self.prev_pause_down = False
        self.prev_mode_down = False
        self.paused = False
        self.mode = "FARM"

    def check_pause_toggle(self):
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

    def check_mode_toggle(self):
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


class Aion2Companion:
    def __init__(self, mode="FARM"):
        self.geom = DynamicWindowGeometry()
        self.vision = AionVision(self.geom)
        self.motor = AionMotor(self.geom)
        self.speech = AionSpeechBridge()
        self.hotkeys = GlobalHotkeyManager()
        self.hotkeys.mode = mode
        self.running = True

        self.last_hp = 100.0
        self.last_mp = 100.0
        self.combat_start = 0.0
        self.kills = 0
        self.roam_count = 0

        self.last_quest_click = 0.0
        self.last_npc_interact = 0.0

        self.skill_cooldowns = {
            KEY_SKILL_1: 1.5,
            KEY_SKILL_2: 3.0,
            KEY_SKILL_3: 5.0,
            KEY_SKILL_4: 7.5,
        }
        self.skill_last_used = {k: 0.0 for k in self.skill_cooldowns}

    def execute_combat_rotation(self, mp):
        now = time.time()
        self.motor.shoot_left()
        if mp > 20.0:
            self.motor.shoot_right()

        for skill, cd in [(KEY_SKILL_3, 5.0), (KEY_SKILL_2, 3.0), (KEY_SKILL_1, 1.5), (KEY_SKILL_4, 7.5)]:
            if (now - self.skill_last_used[skill]) >= cd:
                self.motor.press_key(skill, delay=80)
                self.skill_last_used[skill] = now
                break

    def run(self):
        self.motor.focus_game()
        time.sleep(0.5)

        self.speech.speak(f"Aion 2 Begleiter aktiv! Modus: {self.hotkeys.mode}.", force=True)

        print("\n" + "="*75)
        print("  🏹 AION 2 TACTICAL COMPANION & QUEST SYSTEM")
        print(f"  • Modus: {self.hotkeys.mode}  ([F8] drücken zum Wechseln: FARM <-> QUEST)")
        print(f"  • Fenster erkannt: {self.geom.w}x{self.geom.h} an ({self.geom.ax}, {self.geom.ay})")
        print("  🎮 PAUSE: Drücke [F10] oder [Pause] im Spiel zum Pausieren/Fortsetzen.")
        print("  Tipp: Drücke Strg+C zum Beenden.")
        print("="*75 + "\n")

        while self.running:
            try:
                # Hotkeys
                if self.hotkeys.check_pause_toggle():
                    if self.hotkeys.paused:
                        self.motor.stop_movement()
                        self.speech.speak("Aion pausiert! Du hast die Kontrolle, Tobias.", force=True)
                        print("\n  ⏸️  [PAUSIERT] Aion pausiert — Tobias steuert selbst! ([F10] zum Fortsetzen)\n")
                    else:
                        self.speech.speak(f"Aion setzt fort! Modus: {self.hotkeys.mode}", force=True)
                        print(f"\n  ▶️  [FORTGESETZT] Aion übernimmt wieder! Modus: {self.hotkeys.mode}\n")

                if self.hotkeys.check_mode_toggle():
                    self.speech.speak(f"Modus gewechselt zu {self.hotkeys.mode}!", force=True)
                    print(f"\n  🔄 [MODUS-WECHSEL] Neuer Modus: {self.hotkeys.mode} ([F8] zum Umschalten)\n")

                if self.hotkeys.paused:
                    hp = self.vision.get_player_hp()
                    mp = self.vision.get_player_mp()
                    sys.stdout.write(f"\r  ⏸️  [PAUSE] HP: {hp:5.1f}% | MP: {mp:5.1f}% | [F10/Pause zum Fortsetzen]   ")
                    sys.stdout.flush()
                    time.sleep(0.12)
                    continue

                self.motor.focus_game()

                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # Potions
                if 0.0 < hp < 40.0:
                    self.motor.press_key(KEY_POTION_HP, delay=90)
                    time.sleep(0.15)
                if 0.0 < mp < 20.0:
                    self.motor.press_key(KEY_POTION_MP, delay=90)
                    time.sleep(0.15)

                if target_hp is None:
                    if self.combat_start > 0:
                        self.kills += 1
                        self.speech.speak(f"Ziel eliminiert! Kills: {self.kills}. Sammle Beute.")
                        print(f"\n[AION 2] ⭐ Mob #{self.kills} besiegt! Gehe zur Beute...")
                        self.motor.walk_forward(duration=0.9)
                        self.motor.loot()
                        self.motor.step_back(duration=0.4)
                        self.combat_start = 0.0

                        if self.hotkeys.mode == "QUEST":
                            time.sleep(0.3)
                            self.motor.click_quest_tracker()
                            self.last_quest_click = time.time()

                    now = time.time()
                    if self.hotkeys.mode == "QUEST":
                        if (now - self.last_quest_click) > 7.0:
                            sys.stdout.write("\r[QUEST] 📜 Klicke Quest Tracker für Auto-Lauf...                           ")
                            sys.stdout.flush()
                            self.motor.click_quest_tracker()
                            self.last_quest_click = now

                        if (now - self.last_npc_interact) > 2.5:
                            sys.stdout.write("\r[QUEST] 💬 NPC-Interaktion / Dialoge ([F] / [Space])...                    ")
                            sys.stdout.flush()
                            self.motor.interact_npc()
                            self.last_npc_interact = now

                        time.sleep(0.18)

                    else:
                        sys.stdout.write(f"\r[FARM] Suche Ziel mit [Tab]... (Versuch {self.roam_count+1}) [Kills: {self.kills}]   ")
                        sys.stdout.flush()
                        self.motor.press_key(KEY_TARGET, delay=70)
                        time.sleep(0.28)
                        self.roam_count += 1

                        if self.roam_count >= 3:
                            sys.stdout.write("\r[FARM] Gehe vorwärts ([W])...                                             ")
                            sys.stdout.flush()
                            self.motor.walk_forward(duration=0.55)
                            self.roam_count = 0
                            time.sleep(0.2)

                else:
                    self.roam_count = 0
                    now = time.time()
                    if self.combat_start == 0.0:
                        self.combat_start = now
                        self.speech.speak("Ziel erfasst! Kampfeinsatz gestartet.")

                    # Kiting check
                    if (now - self.combat_start) > 2.5 or (self.last_hp - hp) > 3.0:
                        self.motor.step_back(duration=0.35)

                    self.execute_combat_rotation(mp)
                    time.sleep(0.18)

                    status = f"\r[{self.hotkeys.mode} COMBAT] Ziel-HP: {target_hp:5.1f}% | Kills: {self.kills} | Spieler-HP: {hp:4.0f}% MP: {mp:4.0f}%"
                    sys.stdout.write(status)
                    sys.stdout.flush()

                    self.last_hp = hp
                    self.last_mp = mp

            except KeyboardInterrupt:
                break
            except Exception:
                time.sleep(0.25)

        self.speech.speak(f"Aion 2 Begleiter beendet. {self.kills} Kills erzielt!", force=True)
        print("\n\n[Aion 2] Beendet.\n")


if __name__ == "__main__":
    mode = "FARM"
    args = [a.lower().lstrip("-") for a in sys.argv[1:]]
    if "quest" in args:
        mode = "QUEST"
    elif "farm" in args or "autofarm" in args:
        mode = "FARM"
    elif "copilot" in args:
        mode = "COPILOT"

    bot = Aion2Companion(mode=mode)
    bot.run()
