#!/usr/bin/env python3
# ==============================================================================
# AION 2 AUTONOMOUS COMPANION & AUTO-FARM BOT (MeinOS AI System v2.0)
# ==============================================================================
# Tactical Ranger Combat & Kiting System:
#   1. Left Mouse Button: Continuous primary bow fire (Dauerfeuer)
#   2. Right Mouse Button: Heavy shot continuous fire until MP < 20% (then wait)
#   3. Skills 1, 2, 3, 4: Intelligently rotated with individual cooldowns
#   4. Tactical Kiting: STAYS AT DISTANCE! Never charges into mobs in combat;
#      kites backwards with [S] if enemy gets too close.
#   5. Auto-Loot: Walks forward to corpse and presses [F] x3 ONLY after kill.
#   6. Auto-Potions: Emergency HP [7] & MP [8] management.
# ==============================================================================

import os
import sys
import time
import subprocess
import threading
from Xlib import display, X

AION_WINDOW_NAME = "AION2"

# --- 1080p Calibrated HUD Coordinates ---
# 1. Player HP Bar (Red)
HP_X_START = 738
HP_X_END = 924
HP_Y = 975
HP_WIDTH = HP_X_END - HP_X_START  # 186 px

# 2. Player MP Bar (Cyan/Blue)
MP_X_START = 995
MP_X_END = 1177
MP_Y = 978
MP_WIDTH = MP_X_END - MP_X_START  # 182 px

# 3. Enemy Target Bar (Red, Top Center) - Calibrated to y=52..55
TGT_X_START = 813
TGT_X_END = 1297
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


class AionSpeechBridge:
    """Sends voice thoughts to Aion so she speaks out loud via MeinOS TTS."""
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
    """Reads game HUD with sub-millisecond X11 screen capture."""
    def __init__(self):
        self.disp = display.Display(":0")
        self.root = self.disp.screen().root

    def get_player_hp(self):
        """Returns player HP percentage (0.0 to 100.0)."""
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

    def get_player_mp(self):
        """Returns player MP percentage (0.0 to 100.0)."""
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

    def get_target_hp(self):
        """Returns target HP percentage (0.0 to 100.0), or None if no target selected."""
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
    """Direct hardware input emulator via xdotool with mouse & hold timings."""
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
        """Ensures AION 2 has active OS focus so inputs register."""
        if not self.window_id:
            self.window_id = self.find_window()
        if self.window_id:
            try:
                subprocess.run(["xdotool", "windowactivate", "--sync", self.window_id], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception:
                pass

    def shoot_left(self):
        """Left Mouse Button: Continuous primary bow fire (Dauerfeuer)."""
        try:
            subprocess.run(["xdotool", "mousedown", "1"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.04)
            subprocess.run(["xdotool", "mouseup", "1"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def shoot_right(self):
        """Right Mouse Button: Heavy shot continuous fire (uses Mana)."""
        try:
            subprocess.run(["xdotool", "mousedown", "3"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.04)
            subprocess.run(["xdotool", "mouseup", "3"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def press_key(self, key, delay=80):
        """Presses skill key with a reliable 80ms hold duration."""
        try:
            subprocess.run(["xdotool", "key", "--delay", str(delay), key], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def step_back(self, duration=0.35):
        """Kites backwards with [S] to keep distance from charging monsters."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def walk_forward(self, duration=0.6):
        """Steps forward with [W] ONLY when roaming or collecting loot."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def stop_movement(self):
        """Ensures all movement keys are released so the character stands still."""
        try:
            subprocess.run(["xdotool", "keyup", KEY_FORWARD, "keyup", KEY_BACKWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def loot(self):
        """Taps [F] 3 times to pick up all monster drops."""
        for _ in range(3):
            self.press_key(KEY_LOOT, delay=80)
            time.sleep(0.25)


class Aion2Companion:
    """Tactical Ranger Combat Bot for Aion 2."""
    def __init__(self, mode="copilot"):
        self.vision = AionVision()
        self.motor = AionMotor()
        self.speech = AionSpeechBridge()
        self.mode = mode
        self.running = True

        self.last_hp_warn = 0.0
        self.last_mp_warn = 0.0
        self.kills = 0
        self.roam_attempts = 0

        # Skill cooldown tracking
        self.skills = [KEY_SKILL_1, KEY_SKILL_2, KEY_SKILL_3, KEY_SKILL_4]
        self.skill_cooldowns = {
            KEY_SKILL_1: 1.5,  # Quick shot / skill 1
            KEY_SKILL_2: 3.0,  # Piercing shot / skill 2
            KEY_SKILL_3: 5.0,  # Heavy arrow / skill 3
            KEY_SKILL_4: 7.5,  # Stun / ultimate / skill 4
        }
        self.skill_last_used = {k: 0.0 for k in self.skills}
        self.combat_start_time = 0.0
        self.last_kite_time = 0.0

    def run(self):
        self.motor.focus_game()
        time.sleep(0.5)

        if self.mode == "autofarm":
            self.speech.speak("Auto-Farm aktiviert! Dauerfeuer L-Maus aktiv. R-Maus bei vollem Mana. Ich halte strikt Distanz!", force=True)
            print("\n" + "="*75)
            print("  🏹 AION 2 TACTICAL RANGER BOT — AKTIVIERT")
            print("  - L-Maus: Dauerfeuer Basis-Bogenschuss")
            print("  - R-Maus: Dauerfeuer Spezialschuss (nur bei MP > 20%, stoppt bei OOM)")
            print("  - Skills 1, 2, 3, 4: Intelligente Rotation mit individuellen Cooldowns")
            print("  - Distanz-Management: Bleibt auf Distanz! Kited zurück mit [S] bei Nahkampf")
            print("  - Beute: Läuft nach Kill zum Loot und drückt [F] x3")
            print("  Tipp: Drücke Strg+C zum Beenden.")
            print("="*75 + "\n")
        else:
            self.speech.speak("Co-Pilot aktiv! Ich überwache deine Vitalwerte und gebe Rückendeckung.", force=True)
            print("\n" + "="*75)
            print("  🛡️  AION 2 CO-PILOT — AKTIVIERT")
            print("  Modus: CO-PILOT (Live HUD-Analyse, Sprachwarnungen & Auto-Potions)")
            print("  Tipp: Starte mit 'aion2-companion --farm' für automatischen Kampf!")
            print("="*75 + "\n")

        while self.running:
            try:
                self.motor.focus_game()

                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # --- 1. Vitality Management (HP & MP) ---
                if 0.0 < hp < 40.0:
                    if time.time() - self.last_hp_warn > 5.0:
                        self.speech.speak(f"Achtung Tobias! Leben bei {hp:.0f} Prozent. Ich werfe Heiltrank ein!")
                        self.last_hp_warn = time.time()
                    self.motor.press_key(KEY_POTION_HP, delay=90)
                    time.sleep(0.2)

                if 0.0 < mp < 20.0:
                    if time.time() - self.last_mp_warn > 8.0:
                        self.speech.speak(f"Mana fast leer ({mp:.0f} Prozent)! Pausiere R-Maus und nehme Manatrank.")
                        self.last_mp_warn = time.time()
                    self.motor.press_key(KEY_POTION_MP, delay=90)
                    time.sleep(0.2)

                # --- 2. Co-Pilot Mode ---
                if self.mode == "copilot":
                    if target_hp is not None:
                        status_line = f"\r[CO-PILOT] HP: {hp:5.1f}% | MP: {mp:5.1f}% | Ziel HP: {target_hp:5.1f}% [Im Kampf]  "
                    else:
                        status_line = f"\r[CO-PILOT] HP: {hp:5.1f}% | MP: {mp:5.1f}% | Kein Ziel anvisiert        "
                    sys.stdout.write(status_line)
                    sys.stdout.flush()
                    time.sleep(0.2)

                # --- 3. Tactical Auto-Farm Mode ---
                elif self.mode == "autofarm":
                    if target_hp is None:
                        # No target: search with Tab
                        self.combat_start_time = 0.0
                        sys.stdout.write(f"\r[AUTO-FARM] Suche Ziel ([Tab])... (Versuch {self.roam_attempts+1})                      ")
                        sys.stdout.flush()
                        self.motor.press_key(KEY_TARGET, delay=70)
                        time.sleep(0.3)
                        self.roam_attempts += 1

                        # If after 3 Tabs still no target: step forward slightly (0.5s) to find mobs
                        if self.roam_attempts >= 3:
                            sys.stdout.write("\r[AUTO-FARM] Kein Ziel in Sicht -> Kurzer Schritt vor ([W])...        ")
                            sys.stdout.flush()
                            self.motor.walk_forward(duration=0.5)
                            self.roam_attempts = 0
                            time.sleep(0.2)
                    else:
                        # TARGET ACQUIRED!
                        self.roam_attempts = 0
                        now = time.time()
                        if self.combat_start_time == 0.0:
                            self.combat_start_time = now

                        # 1. DISTANCE CONTROL: Immediately stop forward movement!
                        self.motor.stop_movement()

                        # 2. TACTICAL KITING: If monster closed in (combat running > 3.0s), kite backwards with [S]
                        if (now - self.combat_start_time) > 3.0 and (now - self.last_kite_time) > 4.0:
                            sys.stdout.write(f"\r[AUTO-FARM] 🏃 Monster nähert sich -> Kiting Rückschritt ([S])...       ")
                            sys.stdout.flush()
                            self.motor.step_back(duration=0.4)
                            self.last_kite_time = now
                            time.sleep(0.1)

                        # 3. DAUERFEUER L-MAUS: Always fire primary bow shot!
                        self.motor.shoot_left()

                        # 4. DAUERFEUER R-MAUS: Fire heavy shot ONLY while MP > 20%
                        r_fire_status = "R-Maus (Pause)"
                        if mp > 20.0:
                            self.motor.shoot_right()
                            r_fire_status = "R-Maus (FEUER)"

                        # 5. SKILL ROTATION 1, 2, 3, 4: Fire any skill that is off cooldown
                        fired_skill = "—"
                        for s in self.skills:
                            cd = self.skill_cooldowns[s]
                            if (now - self.skill_last_used[s]) >= cd:
                                self.motor.press_key(s, delay=80)
                                self.skill_last_used[s] = now
                                fired_skill = f"Skill [{s}]"
                                break

                        status = f"\r[AUTO-FARM] 🎯 Ziel {target_hp:5.1f}% | L-Dauerfeuer | {r_fire_status} | {fired_skill:10s}"
                        sys.stdout.write(status)
                        sys.stdout.flush()
                        time.sleep(0.18)

                        # Check if target is defeated
                        check_target = self.vision.get_target_hp()
                        if check_target is None:
                            self.kills += 1
                            self.speech.speak(f"Ziel eliminiert! Kills: {self.kills}. Gehe hin und sammle Beute.")
                            sys.stdout.write(f"\n[AUTO-FARM] ⭐ Ziel besiegt! Gehe Beute einsammeln ([W] + [F])...\n")
                            sys.stdout.flush()
                            # Walk towards defeated monster and loot
                            self.motor.walk_forward(duration=1.1)
                            self.motor.loot()
                            # Step back to re-establish combat range
                            self.motor.step_back(duration=0.5)
                            time.sleep(0.3)

            except KeyboardInterrupt:
                break
            except Exception as e:
                time.sleep(0.3)

        self.speech.speak("Aion 2 Begleiter beendet. Ausgezeichnet gekämpft!", force=True)
        print("\n\n[Aion Companion] Beendet.\n")


if __name__ == "__main__":
    mode_arg = "copilot"
    args = [a.lower().lstrip("-") for a in sys.argv[1:]]
    if any(a in ["farm", "autofarm", "bot", "auto", "f"] for a in args):
        mode_arg = "autofarm"

    companion = Aion2Companion(mode=mode_arg)
    companion.run()
