#!/usr/bin/env python3
# ==============================================================================
# AION 2 GAMING CONTROL & TELEMETRY GUI (MeinOS AI System)
# ==============================================================================
# Linux GUI window featuring:
#   1. Real-time Status Badges: Running / Paused / Mode (Quest vs Farm)
#   2. Live HP, MP & Enemy Target Visual Meters
#   3. Interactive Control Buttons:
#      - ▶️ Start / Weiterspielen
#      - ⏸️ Pause / Resume [F10 / Pause]
#      - 📜 Quest-Modus [F8] (Auto-Pathing & NPC Dialogues)
#      - 🏹 Auto-Farm Modus [F8] (Mob Hunting & Kiting)
#      - 🔄 Unstuck / Befreien (Jump + Lateral Strafe)
#      - 🎯 Target Scan [Tab]
#      - 📜 Quest Tracker Click
#      - 💬 NPC Interagieren [F / Space]
#      - 🛑 Beenden
#   4. Live Telemetry & Log Console with Auto-Scroll
#   5. Reinforcement Learning Telemetry (Q-States, Kills, TTK, Epsilon)
# ==============================================================================

import os
import sys
import time
import json
import subprocess
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext

STATUS_FILE = "/tmp/aion_nexus_status.json"
CMD_FILE = "/tmp/aion_nexus_cmd.txt"
LOG_FILE = "/tmp/aion_nexus_log.txt"
MEMORY_FILE = "/root/coding/MeinOS/aion_nexus_combat_memory.json"
GAMER_SCRIPT = "/root/coding/MeinOS/aion_nexus_gamer.py"


class AionGamingGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("AION 2 — NEURAL GAMING & QUEST CONTROL (MeinOS)")
        self.geometry("960x640")
        self.minsize(840, 560)
        self.configure(bg="#0c0e17")

        # Always on Top by default so game window cannot bury the GUI!
        self.is_topmost = True
        self.attributes("-topmost", True)

        # Command queue writer
        self.last_log_pos = 0

        self._init_styles()
        self._build_header()
        self._build_main_layout()
        self._build_footer()

        # Start periodic update loops
        self.after(200, self._update_telemetry)
        self.after(300, self._update_logs)

    def _init_styles(self):
        self.style = ttk.Style(self)
        self.style.theme_use("clam")

    def _build_header(self):
        hdr = tk.Frame(self, bg="#131625", height=60, bd=0, relief="flat")
        hdr.pack(fill="x", side="top", padx=8, pady=(8, 4))

        # Title & Subtitle
        title_box = tk.Frame(hdr, bg="#131625")
        title_box.pack(side="left", padx=12, pady=6)
        lbl_t = tk.Label(title_box, text="🎮 AION 2 NEURAL AGENT", font=("Helvetica", 14, "bold"), fg="#00e5ff", bg="#131625")
        lbl_t.pack(anchor="w")
        lbl_sub = tk.Label(title_box, text="MeinOS Autonomous Reinforcement Learning & Quest System", font=("Helvetica", 9), fg="#8c93b0", bg="#131625")
        lbl_sub.pack(anchor="w")

        # Badges
        self.badge_box = tk.Frame(hdr, bg="#131625")
        self.badge_box.pack(side="right", padx=12, pady=8)

        # Pin On Top Toggle Button
        self.btn_pin = tk.Button(self.badge_box, text="📌 PIN: ON (Vordergrund)", font=("Helvetica", 9, "bold"),
                                 fg="#00e5ff", bg="#152f47", activebackground="#00e5ff", activeforeground="#000000",
                                 bd=1, relief="groove", cursor="hand2", padx=8, pady=2, command=self._toggle_topmost)
        self.btn_pin.pack(side="right", padx=6)

        self.lbl_status = tk.Label(self.badge_box, text="⚫ PRÜFE STATUS...", font=("Helvetica", 10, "bold"), fg="#ffffff", bg="#2a2e45", padx=10, pady=4)
        self.lbl_status.pack(side="right", padx=4)

        self.lbl_mode = tk.Label(self.badge_box, text="MODE: UNBEKANNT", font=("Helvetica", 10, "bold"), fg="#ffd700", bg="#3a3015", padx=10, pady=4)
        self.lbl_mode.pack(side="right", padx=4)

    def _toggle_topmost(self):
        self.is_topmost = not self.is_topmost
        self.attributes("-topmost", self.is_topmost)
        if self.is_topmost:
            self.lift()
            self.btn_pin.config(text="📌 PIN: ON (Vordergrund)", fg="#00e5ff", bg="#152f47")
            self._log_local("[GUI] Fenster bleibt immer im Vordergrund (Always-On-Top)", "info")
        else:
            self.btn_pin.config(text="📌 PIN: OFF", fg="#888888", bg="#202438")
            self._log_local("[GUI] Always-On-Top deaktiviert", "dim")

    def _build_main_layout(self):
        main_pane = tk.PanedWindow(self, orient="horizontal", bg="#0c0e17", bd=0, sashwidth=4, sashrelief="flat")
        main_pane.pack(fill="both", expand=True, padx=8, pady=4)

        # Left Column: Meters + Options Buttons
        left_col = tk.Frame(main_pane, bg="#131625", width=340)
        left_col.pack_propagate(False)
        main_pane.add(left_col, minsize=320)

        # Meters Box
        m_frame = tk.LabelFrame(left_col, text=" VITALWERTE & ZIEL ", font=("Helvetica", 9, "bold"), fg="#00e5ff", bg="#131625", bd=1, relief="groove")
        m_frame.pack(fill="x", padx=8, pady=(6, 4))

        # Player HP
        hp_row = tk.Frame(m_frame, bg="#131625")
        hp_row.pack(fill="x", padx=6, pady=2)
        tk.Label(hp_row, text="SPIELER HP:", font=("Helvetica", 9, "bold"), fg="#ff5252", bg="#131625", width=11, anchor="w").pack(side="left")
        self.lbl_hp_val = tk.Label(hp_row, text="100.0%", font=("Helvetica", 9, "bold"), fg="#ffffff", bg="#131625", width=7, anchor="e")
        self.lbl_hp_val.pack(side="right")
        self.cv_hp = tk.Canvas(hp_row, height=12, bg="#2a1518", highlightthickness=0)
        self.cv_hp.pack(fill="x", expand=True, padx=4)

        # Player MP
        mp_row = tk.Frame(m_frame, bg="#131625")
        mp_row.pack(fill="x", padx=6, pady=2)
        tk.Label(mp_row, text="SPIELER MP:", font=("Helvetica", 9, "bold"), fg="#40c4ff", bg="#131625", width=11, anchor="w").pack(side="left")
        self.lbl_mp_val = tk.Label(mp_row, text="100.0%", font=("Helvetica", 9, "bold"), fg="#ffffff", bg="#131625", width=7, anchor="e")
        self.lbl_mp_val.pack(side="right")
        self.cv_mp = tk.Canvas(mp_row, height=12, bg="#102535", highlightthickness=0)
        self.cv_mp.pack(fill="x", expand=True, padx=4)

        # Enemy Target HP
        tgt_row = tk.Frame(m_frame, bg="#131625")
        tgt_row.pack(fill="x", padx=6, pady=(2, 6))
        tk.Label(tgt_row, text="GEGNER HP:", font=("Helvetica", 9, "bold"), fg="#ff9100", bg="#131625", width=11, anchor="w").pack(side="left")
        self.lbl_tgt_val = tk.Label(tgt_row, text="KEIN ZIEL", font=("Helvetica", 9, "bold"), fg="#888888", bg="#131625", width=9, anchor="e")
        self.lbl_tgt_val.pack(side="right")
        self.cv_tgt = tk.Canvas(tgt_row, height=12, bg="#332010", highlightthickness=0)
        self.cv_tgt.pack(fill="x", expand=True, padx=4)

        # Option Buttons Box
        b_frame = tk.LabelFrame(left_col, text=" KONTROLL-OPTIONEN ", font=("Helvetica", 9, "bold"), fg="#ffd700", bg="#131625", bd=1, relief="groove")
        b_frame.pack(fill="both", expand=True, padx=8, pady=(4, 6))

        # Buttons Grid
        btn_cfg = [
            ("▶️ Start Aion Nexus", "#00b0ff", "#002845", self._cmd_start),
            ("⏸️ Pause / Weiter [F10]", "#ffb300", "#3d2a00", self._cmd_pause),
            ("📜 Quest-Modus [F8]", "#ffd700", "#382f05", self._cmd_mode_quest),
            ("🏹 Auto-Farm [F8]", "#00e676", "#043318", self._cmd_mode_farm),
            ("🔄 Unstuck (Sprung+Ausweichen)", "#b388ff", "#291847", self._cmd_unstuck),
            ("🎯 Gegner erfassen [Tab]", "#ff80ab", "#3b1422", self._cmd_target),
            ("📜 Quest Tracker Anklicken", "#ffe57f", "#3d3305", self._cmd_click_quest),
            ("💬 NPC Dialog [F / Space]", "#80d8ff", "#092f42", self._cmd_interact_npc),
            ("🛑 Aion Beenden", "#ff5252", "#420d12", self._cmd_stop),
        ]

        for text, fg, bg, cmd in btn_cfg:
            btn = tk.Button(b_frame, text=text, font=("Helvetica", 9, "bold"), fg=fg, bg=bg,
                            activebackground=fg, activeforeground="#000000", relief="groove", bd=1,
                            cursor="hand2", command=cmd, height=1)
            btn.pack(fill="x", padx=6, pady=2)

        # Right Column: Live Logs & Telemetry
        right_col = tk.Frame(main_pane, bg="#131625")
        main_pane.add(right_col, minsize=480)

        log_hdr = tk.Frame(right_col, bg="#131625")
        log_hdr.pack(fill="x", padx=8, pady=(6, 2))
        tk.Label(log_hdr, text="📋 LIVE-TELEMETRIE & LOG-STREAM", font=("Helvetica", 10, "bold"), fg="#00e5ff", bg="#131625").pack(side="left")

        # Log filter buttons
        tk.Button(log_hdr, text="🧹 Clear", font=("Helvetica", 8), fg="#aaaaaa", bg="#202438", bd=0, padx=6,
                  command=self._clear_logs).pack(side="right", padx=2)

        self.txt_log = scrolledtext.ScrolledText(right_col, bg="#080910", fg="#d0d5e8", font=("Courier", 9),
                                                  insertbackground="#00e5ff", relief="flat", bd=0)
        self.txt_log.pack(fill="both", expand=True, padx=8, pady=4)

        # Configure color tags for log text
        self.txt_log.tag_config("combat", foreground="#ff5252")
        self.txt_log.tag_config("quest", foreground="#ffd700")
        self.txt_log.tag_config("kill", foreground="#00e676")
        self.txt_log.tag_config("speech", foreground="#b388ff")
        self.txt_log.tag_config("info", foreground="#40c4ff")
        self.txt_log.tag_config("dim", foreground="#606680")

    def _build_footer(self):
        ftr = tk.Frame(self, bg="#131625", height=45, bd=0)
        ftr.pack(fill="x", side="bottom", padx=8, pady=(2, 8))

        self.lbl_stats = tk.Label(ftr, text="Kills: 0 | Avg TTK: --s | Q-States: 0 | Epsilon: --%",
                                  font=("Courier", 9, "bold"), fg="#00e5ff", bg="#131625")
        self.lbl_stats.pack(side="left", padx=10, pady=8)

        self.lbl_thought = tk.Label(ftr, text="Aion: Bereit.", font=("Helvetica", 9, "italic"), fg="#b388ff", bg="#131625")
        self.lbl_thought.pack(side="right", padx=10, pady=8)

    # --- Commands ---
    def _send_cmd(self, cmd_text: str):
        try:
            with open(CMD_FILE, "w") as f:
                f.write(cmd_text.strip() + "\n")
            self._log_local(f"[GUI COMMAND] Gesendet: {cmd_text}", "info")
        except Exception as e:
            self._log_local(f"[GUI ERROR] Konnte Befehl nicht senden: {e}", "combat")

    def _cmd_start(self):
        # Check if already running
        try:
            out = subprocess.check_output(["pgrep", "-f", GAMER_SCRIPT], text=True).strip()
            if out:
                self._send_cmd("RESUME")
                return
        except Exception:
            pass
        subprocess.Popen(["python3", GAMER_SCRIPT, "--quest"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self._log_local("[GUI] Aion Nexus neu gestartet im Hintergrund!", "info")

    def _cmd_pause(self):
        self._send_cmd("TOGGLE_PAUSE")

    def _cmd_mode_quest(self):
        self._send_cmd("MODE_QUEST")

    def _cmd_mode_farm(self):
        self._send_cmd("MODE_FARM")

    def _cmd_unstuck(self):
        self._send_cmd("UNSTUCK")

    def _cmd_target(self):
        self._send_cmd("TARGET")

    def _cmd_click_quest(self):
        self._send_cmd("CLICK_QUEST")

    def _cmd_interact_npc(self):
        self._send_cmd("INTERACT_NPC")

    def _cmd_stop(self):
        self._send_cmd("STOP")

    def _clear_logs(self):
        self.txt_log.delete("1.0", "end")

    def _log_local(self, msg: str, tag: str = "dim"):
        self.txt_log.insert("end", f"{msg}\n", tag)
        self.txt_log.see("end")

    # --- Periodic UI Update ---
    def _draw_bar(self, canvas, pct: float, fill_color: str):
        canvas.delete("all")
        w = canvas.winfo_width()
        h = canvas.winfo_height()
        if w > 1:
            pw = int((max(0.0, min(100.0, pct)) / 100.0) * w)
            canvas.create_rectangle(0, 0, pw, h, fill=fill_color, width=0)

    def _update_telemetry(self):
        # Read status JSON if available
        if os.path.exists(STATUS_FILE):
            try:
                with open(STATUS_FILE, "r") as f:
                    data = json.load(f)

                running = data.get("running", False)
                paused = data.get("paused", False)
                mode = data.get("mode", "FARM")
                hp = data.get("player_hp", 100.0)
                mp = data.get("player_mp", 100.0)
                target_hp = data.get("target_hp", None)
                kills = data.get("kills", 0)
                avg_ttk = data.get("avg_ttk", 0.0)
                eps = data.get("epsilon", 0.35)
                q_states = data.get("states_learned", 0)
                thought = data.get("last_thought", "")

                # Update Status Badge
                if not running:
                    self.lbl_status.config(text="🔴 OFFLINE", bg="#4a151b", fg="#ffffff")
                elif paused:
                    self.lbl_status.config(text="⏸️ PAUSIERT (Tobias steuert)", bg="#614300", fg="#ffecb3")
                else:
                    self.lbl_status.config(text="🟢 AUTONOM AKTIV", bg="#0a4020", fg="#b9f6ca")

                # Mode Badge
                if mode == "QUEST":
                    self.lbl_mode.config(text="📜 QUEST-MODUS", bg="#524005", fg="#ffe082")
                else:
                    self.lbl_mode.config(text="🏹 FARM-MODUS", bg="#0a3d24", fg="#a7f3d0")

                # Meters
                self.lbl_hp_val.config(text=f"{hp:.1f}%")
                self._draw_bar(self.cv_hp, hp, "#ff3344" if hp > 30 else "#ff1122")

                self.lbl_mp_val.config(text=f"{mp:.1f}%")
                self._draw_bar(self.cv_mp, mp, "#00b0ff")

                if target_hp is not None:
                    self.lbl_tgt_val.config(text=f"{target_hp:.1f}%", fg="#ff9100")
                    self._draw_bar(self.cv_tgt, target_hp, "#ff5500")
                else:
                    self.lbl_tgt_val.config(text="KEIN ZIEL", fg="#666666")
                    self._draw_bar(self.cv_tgt, 0.0, "#332010")

                # Stats Footer
                self.lbl_stats.config(
                    text=f"Kills: {kills} | Avg TTK: {avg_ttk:.1f}s | Q-States: {q_states} | Epsilon: {eps*100:.1f}%"
                )
                if thought:
                    self.lbl_thought.config(text=f"Aion: {thought[:55]}...")

            except Exception:
                pass
        else:
            # Check process
            try:
                out = subprocess.check_output(["pgrep", "-f", GAMER_SCRIPT], text=True).strip()
                if out:
                    self.lbl_status.config(text="🟢 PROZESS LÄUFT", bg="#0a4020", fg="#b9f6ca")
                else:
                    self.lbl_status.config(text="🔴 OFFLINE", bg="#4a151b", fg="#ffffff")
            except Exception:
                self.lbl_status.config(text="🔴 OFFLINE", bg="#4a151b", fg="#ffffff")

        if self.is_topmost:
            try:
                self.attributes("-topmost", True)
            except Exception:
                pass

        self.after(300, self._update_telemetry)

    def _update_logs(self):
        if os.path.exists(LOG_FILE):
            try:
                with open(LOG_FILE, "r") as f:
                    f.seek(self.last_log_pos)
                    lines = f.readlines()
                    self.last_log_pos = f.tell()

                for line in lines:
                    line_clean = line.strip()
                    if not line_clean:
                        continue
                    tag = "dim"
                    if "Kill #" in line_clean or "⭐" in line_clean:
                        tag = "kill"
                    elif "COMBAT" in line_clean or "Ziel:" in line_clean:
                        tag = "combat"
                    elif "QUEST" in line_clean or "NPC" in line_clean:
                        tag = "quest"
                    elif "[AION]" in line_clean:
                        tag = "speech"
                    elif "FARM" in line_clean or "SCOUT" in line_clean:
                        tag = "info"

                    self.txt_log.insert("end", f"{line_clean}\n", tag)

                if lines:
                    self.txt_log.see("end")
            except Exception:
                pass

        self.after(400, self._update_logs)


def main():
    app = AionGamingGUI()
    app.mainloop()


if __name__ == "__main__":
    main()

