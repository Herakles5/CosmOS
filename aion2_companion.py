#!/usr/bin/env python3
# ==============================================================================
# AION 2 AUTONOMOUS COMPANION & AUTO-FARM BOT (MeinOS AI System)
# ==============================================================================
# Connects Aion (the MeinOS companion) directly to NCSoft's AION 2:
#   1. Perception: Sub-millisecond HUD vision (HP/MP & 100% calibrated Target Bar)
#   2. Action: Real key-hold emulation via xdotool (Anti-Cheat-Safe)
#   3. Movement & Combat:
#      - Auto-Roam: Steps forward with [W] when searching for monsters
#      - Lock-On: Presses [Tab] to acquire target
#      - Bow Combo: [1], [2], [3], [4] and Left-Click auto-shoot
#      - Auto-Loot: Steps to defeated monster and presses [F] x3
#      - Emergency: Auto-drinks HP [7] & MP [8] potions
#   4. Voice: Real-time commentary through Aion Bridge
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

# 3. Enemy Target Bar (Red, Top Center)
TGT_X_START = 813
TGT_X_END = 1297
TGT_Y_ROWS = [52, 53, 54, 55]
TGT_WIDTH = TGT_X_END - TGT_X_START  # 484 px

# Hotkeys
KEY_TARGET = "Tab"
KEY_ATTACK_1 = "1"
KEY_ATTACK_2 = "2"
KEY_ATTACK_3 = "3"
KEY_ATTACK_4 = "4"
KEY_LOOT = "f"
KEY_POTION_HP = "7"
KEY_POTION_MP = "8"
KEY_FORWARD = "w"


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
            if total_red < 30:
                return None
            pct = (total_red / total_sampled) * 100.0 * 2.5
            return max(1.0, min(100.0, pct))
        except Exception:
            return None


class AionMotor:
    """Direct hardware input emulator via xdotool with proper key-hold delays."""
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

    def press_key(self, key, delay=70):
        """Presses key with a human-like 70ms hold duration."""
        try:
            subprocess.run(["xdotool", "key", "--delay", str(delay), key], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def walk_forward(self, duration=0.8):
        """Holds [W] forward to approach monsters or loot."""
        try:
            subprocess.run(["xdotool", "keydown", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(duration)
            subprocess.run(["xdotool", "keyup", KEY_FORWARD], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def mouse_shoot(self):
        """Clicks left mouse button for basic bow auto-attack."""
        try:
            subprocess.run(["xdotool", "click", "1"], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    def loot(self):
        """Taps [F] 3 times to pick up all monster drops."""
        for _ in range(3):
            self.press_key(KEY_LOOT, delay=80)
            time.sleep(0.25)


class Aion2Companion:
    """Main companion logic controlling Co-Pilot and Auto-Farm."""
    def __init__(self, mode="copilot"):
        self.vision = AionVision()
        self.motor = AionMotor()
        self.speech = AionSpeechBridge()
        self.mode = mode  # "copilot" or "autofarm"
        self.running = True

        self.last_hp_warn = 0.0
        self.last_mp_warn = 0.0
        self.skill_index = 0
        self.kills = 0
        self.roam_attempts = 0

    def run(self):
        # 1. Activate game window
        self.motor.focus_game()
        time.sleep(0.5)

        if self.mode == "autofarm":
            self.speech.speak("Aion 2 Auto-Farm Modus aktiviert! Ich übernehme Zielen, Laufen und Bogenschuss-Combos.", force=True)
            print("\n" + "="*70)
            print("  🏹 AION 2 AUTONOMOUS FARM BOT — AKTIVIERT")
            print("  Modus: AUTO-FARM (Tab-Targeting, Pfeil-Combos, Laufen & Looten)")
            print("  Tipp: Drücke Strg+C zum Beenden.")
            print("="*70 + "\n")
        else:
            self.speech.speak("Aion 2 Co-Pilot aktiviert! Ich überwache dein Leben und gebe dir Rückendeckung.", force=True)
            print("\n" + "="*70)
            print("  🛡️  AION 2 CO-PILOT — AKTIVIERT")
            print("  Modus: CO-PILOT (Live HUD-Analyse, Sprach-Warnungen & Notfall-Heilung)")
            print("  Tipp: Starte mit 'aion2-companion --farm' für selbstständigen Kampf!")
            print("="*70 + "\n")

        rotation = [KEY_ATTACK_1, KEY_ATTACK_2, KEY_ATTACK_1, KEY_ATTACK_3, KEY_ATTACK_4]

        while self.running:
            try:
                # Always keep game in focus
                self.motor.focus_game()

                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # --- 1. Vitality Management (HP & MP) ---
                if 0.0 < hp < 40.0:
                    if time.time() - self.last_hp_warn > 5.0:
                        self.speech.speak(f"Achtung Tobias! Dein Leben ist auf {hp:.0f} Prozent gesunken. Ich trinke einen Heiltrank!")
                        self.last_hp_warn = time.time()
                    self.motor.press_key(KEY_POTION_HP, delay=90)
                    time.sleep(0.3)

                if 0.0 < mp < 25.0:
                    if time.time() - self.last_mp_warn > 8.0:
                        self.speech.speak(f"Dein Mana ist fast leer ({mp:.0f} Prozent). Setze Manatrank ein.")
                        self.last_mp_warn = time.time()
                    self.motor.press_key(KEY_POTION_MP, delay=90)
                    time.sleep(0.3)

                # --- 2. Co-Pilot Mode ---
                if self.mode == "copilot":
                    if target_hp is not None:
                        status_line = f"\r[CO-PILOT] Spieler HP: {hp:5.1f}% | MP: {mp:5.1f}% | Ziel HP: {target_hp:5.1f}% [Im Kampf]  "
                    else:
                        status_line = f"\r[CO-PILOT] Spieler HP: {hp:5.1f}% | MP: {mp:5.1f}% | Kein Ziel anvisiert        "
                    sys.stdout.write(status_line)
                    sys.stdout.flush()
                    time.sleep(0.2)

                # --- 3. Auto-Farm Mode ---
                elif self.mode == "autofarm":
                    if target_hp is None:
                        # No target: Attempt Tab target lock
                        sys.stdout.write(f"\r[AUTO-FARM] Suche Ziel ([Tab])... (Versuch {self.roam_attempts+1})                      ")
                        sys.stdout.flush()
                        self.motor.press_key(KEY_TARGET, delay=80)
                        time.sleep(0.3)
                        self.roam_attempts += 1

                        # If after 2 Tabs no target was found: walk forward to find mobs!
                        if self.roam_attempts >= 3:
                            sys.stdout.write("\r[AUTO-FARM] Kein Ziel in Reichweite -> Laufe vorwärts ([W])...        ")
                            sys.stdout.flush()
                            self.motor.walk_forward(duration=0.9)
                            self.roam_attempts = 0
                            time.sleep(0.2)
                    else:
                        # Target found! Reset roam
                        self.roam_attempts = 0

                        # Fire bow combo skill
                        skill = rotation[self.skill_index % len(rotation)]
                        self.skill_index += 1
                        sys.stdout.write(f"\r[AUTO-FARM] 🏹 Bekämpfe Ziel ({target_hp:5.1f}% HP) -> Feuere Skill [{skill}]!       ")
                        sys.stdout.flush()

                        # Press combo key & basic shot
                        self.motor.press_key(skill, delay=90)
                        self.motor.mouse_shoot()
                        time.sleep(0.4)

                        # Check if target is defeated
                        check_target = self.vision.get_target_hp()
                        if check_target is None:
                            self.kills += 1
                            self.speech.speak(f"Ziel besiegt! Kills gesamt: {self.kills}. Gehe hin und loote!")
                            sys.stdout.write(f"\n[AUTO-FARM] ⭐ Ziel besiegt! Gehe Beute einsammeln ([W] + [F])...\n")
                            sys.stdout.flush()
                            # Walk towards fallen monster and loot
                            self.motor.walk_forward(duration=1.1)
                            self.motor.loot()
                            time.sleep(0.4)

            except KeyboardInterrupt:
                break
            except Exception as e:
                time.sleep(0.3)

        self.speech.speak("Aion 2 Begleiter beendet. Ausgezeichnet gekämpft, Tobias!", force=True)
        print("\n\n[Aion Companion] Beendet.\n")


if __name__ == "__main__":
    mode_arg = "copilot"
    # Parse CLI flags gracefully (e.g. --farm, -farm, farm, bot, --bot, auto)
    args = [a.lower().lstrip("-") for a in sys.argv[1:]]
    if any(a in ["farm", "autofarm", "bot", "auto", "f"] for a in args):
        mode_arg = "autofarm"

    companion = Aion2Companion(mode=mode_arg)
    companion.run()
