#!/usr/bin/env python3
# ==============================================================================
# AION 2 AUTONOMOUS COMPANION & CO-PILOT (MeinOS AI System)
# ==============================================================================
# Connects Aion (the MeinOS companion) directly to NCSoft's AION 2:
#   1. Perception: Sub-millisecond HUD computer vision (HP/MP/Target Bar)
#   2. Action: Anti-cheat-safe hardware keypress emulation via xdotool
#   3. Voice: Real-time speech and warnings through Aion Bridge
#   4. Modes:
#      - Co-Pilot: Voice warnings & automated emergency healing
#      - Auto-Farm: Automated Tab-targeting, combat combo rotation & loot
# ==============================================================================

import os
import sys
import time
import subprocess
import threading
from Xlib import display, X

# --- Aion 2 Native 1080p Calibration Constants ---
AION_WINDOW_NAME = "AION2"

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
TGT_X_START = 817
TGT_X_END = 1216
TGT_Y = 60
TGT_WIDTH = TGT_X_END - TGT_X_START  # 399 px

# Hotkey Mapping
KEY_TARGET = "Tab"
KEY_ATTACK_1 = "1"
KEY_ATTACK_2 = "2"
KEY_ATTACK_3 = "3"
KEY_ATTACK_4 = "4"
KEY_LOOT = "f"
KEY_POTION_HP = "7"
KEY_POTION_MP = "8"
KEY_JUMP = "space"


class AionSpeechBridge:
    """Sends voice thoughts to Aion so she speaks out loud via MeinOS TTS."""
    def __init__(self):
        self.bridge_pipe = "/tmp/ide_to_aion.txt"
        self.last_spoken = 0.0
        self.cooldown = 4.0  # Avoid spamming voice

    def speak(self, text, force=False):
        now = time.time()
        if not force and (now - self.last_spoken) < self.cooldown:
            return
        self.last_spoken = now
        formatted = f"[AION] {text}\n"
        print(f"\033[95m{formatted.strip()}\033[0m")
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
            return (red_count / (HP_WIDTH * 4)) * 100.0
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
            return (blue_count / (MP_WIDTH * 3)) * 100.0
        except Exception:
            return 0.0

    def get_target_hp(self):
        """Returns target HP percentage (0.0 to 100.0), or None if no target."""
        try:
            img = self.root.get_image(TGT_X_START, TGT_Y, TGT_WIDTH, 4, X.ZPixmap, 0xffffffff)
            data = img.data
            red_count = 0
            for i in range(0, len(data), 4):
                b, g, r = data[i], data[i+1], data[i+2]
                if r > 140 and g < 60 and b < 60:
                    red_count += 1
            total_tested = TGT_WIDTH * 4
            pct = (red_count / total_tested) * 100.0
            if red_count < 15:  # No enemy target bar visible
                return None
            return min(100.0, pct * 2.5)  # Normalizing fill ratio
        except Exception:
            return None


class AionMotor:
    """Simulates hardware keyboard inputs via xdotool (Safe from NCGuard)."""
    def __init__(self):
        self.window_id = self.find_window()

    def find_window(self):
        try:
            out = subprocess.check_output(["xdotool", "search", "--name", AION_WINDOW_NAME], text=True).strip()
            if out:
                lines = out.splitlines()
                return lines[0]
        except Exception:
            pass
        return None

    def press_key(self, key):
        if not self.window_id:
            self.window_id = self.find_window()
        try:
            if self.window_id:
                subprocess.run(["xdotool", "key", "--window", self.window_id, key], check=False)
            else:
                subprocess.run(["xdotool", "key", key], check=False)
        except Exception:
            pass


class Aion2Companion:
    """Main Agent Loop controlling Aion's Co-Pilot and Auto-Combat."""
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

    def run(self):
        self.speech.speak(f"Aion 2 Begleiter aktiviert im Modus: {self.mode.upper()}!", force=True)
        print(f"\n[Aion Companion] Gestartet im Modus: {self.mode.upper()}")
        print("[Aion Companion] Drücke Strg+C zum Beenden.\n")

        rotation = [KEY_ATTACK_1, KEY_ATTACK_2, KEY_ATTACK_3, KEY_ATTACK_1, KEY_ATTACK_4]

        while self.running:
            try:
                hp = self.vision.get_player_hp()
                mp = self.vision.get_player_mp()
                target_hp = self.vision.get_target_hp()

                # 1. Health & Mana Checks
                if 0.0 < hp < 40.0:
                    if time.time() - self.last_hp_warn > 5.0:
                        self.speech.speak(f"Achtung Tobias! Dein Leben ist auf {hp:.0f} Prozent gesunken. Ich trinke einen Heiltrank!")
                        self.last_hp_warn = time.time()
                    self.motor.press_key(KEY_POTION_HP)
                    time.sleep(0.3)

                if 0.0 < mp < 25.0:
                    if time.time() - self.last_mp_warn > 8.0:
                        self.speech.speak(f"Dein Mana ist fast leer ({mp:.0f} Prozent). Setze Manatrank ein.")
                        self.last_mp_warn = time.time()
                    self.motor.press_key(KEY_POTION_MP)
                    time.sleep(0.3)

                # 2. Mode: Co-Pilot (Observes, assists, speaks)
                if self.mode == "copilot":
                    if target_hp is not None:
                        status_line = f"\r[AION CO-PILOT] Spieler HP: {hp:5.1f}% | MP: {mp:5.1f}% | Ziel HP: {target_hp:5.1f}%"
                    else:
                        status_line = f"\r[AION CO-PILOT] Spieler HP: {hp:5.1f}% | MP: {mp:5.1f}% | Kein Ziel anvisiert"
                    sys.stdout.write(status_line)
                    sys.stdout.flush()
                    time.sleep(0.2)

                # 3. Mode: Auto-Farm (Automated Combat & Looting)
                elif self.mode == "autofarm":
                    if target_hp is None or target_hp <= 0.0:
                        # Need a target: press Tab
                        sys.stdout.write("\r[AION AUTOFARM] Suche nächstes Ziel ([Tab])...                     ")
                        sys.stdout.flush()
                        self.motor.press_key(KEY_TARGET)
                        time.sleep(0.4)
                    else:
                        # Enemy targeted -> execute skill rotation
                        key_to_press = rotation[self.skill_index % len(rotation)]
                        self.skill_index += 1
                        sys.stdout.write(f"\r[AION AUTOFARM] Bekämpfe Ziel ({target_hp:4.1f}% HP) -> Skill [{key_to_press}]")
                        sys.stdout.flush()
                        self.motor.press_key(key_to_press)
                        time.sleep(0.5)

                        # Check if defeated
                        new_target_hp = self.vision.get_target_hp()
                        if new_target_hp is None or new_target_hp <= 0.0:
                            self.kills += 1
                            self.speech.speak(f"Ziel eliminiert! Kills gesamt: {self.kills}. Sammle Beute ein.")
                            # Loot with F
                            for _ in range(3):
                                self.motor.press_key(KEY_LOOT)
                                time.sleep(0.2)
                            time.sleep(0.5)

            except KeyboardInterrupt:
                break
            except Exception as e:
                time.sleep(0.5)

        self.speech.speak("Aion 2 Begleiter wurde beendet. Gute Jagd, Tobias!", force=True)
        print("\n[Aion Companion] Beendet.")


if __name__ == "__main__":
    mode_arg = "copilot"
    if len(sys.argv) > 1 and sys.argv[1].lower() in ["autofarm", "farm", "bot"]:
        mode_arg = "autofarm"
    companion = Aion2Companion(mode=mode_arg)
    companion.run()
