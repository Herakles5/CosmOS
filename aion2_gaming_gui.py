#!/usr/bin/env python3
# ==============================================================================
# AION 2 GAMING CONTROL & TELEMETRY GUI (MeinOS AI System)
# ==============================================================================
# Linux GUI window featuring:
#   1. Real-time Status Badges: Running / Paused / Mode (Quest vs Farm)
#   2. Live HP, MP, Enemy Target & Scene Vision (Threats, Skills, Workflows)
#   3. 5 Focused Control Buttons:
#      1) Start Aion Nexus
#      2) Pause / Resume [F10]
#      3) Quest Auto-Run [Alt+1]
#      4) Auto-Farm [F8]
#      5) Stop Aion Nexus
#   4. Live Telemetry & Log Console with Auto-Scroll
#   5. Reinforcement Learning & Workflow Memory Telemetry
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
        self.geometry("600x410+8+8")
        self.minsize(560, 380)
        self.configure(bg="#0c0e17")

        # Keep compact top-left footprint so Always-on-Top never obscures Target HP, Center, or Minimap
        self.is_topmost = True
        self.attributes("-topmost", True)
        self.deiconify()
        self.lift()

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
        hdr = tk.Frame(self, bg="#131625", height=48, bd=0, relief="flat")
        hdr.pack(fill="x", side="top", padx=6, pady=(6, 3))

        # Title & Subtitle
        title_box = tk.Frame(hdr, bg="#131625")
        title_box.pack(side="left", padx=8, pady=4)
        lbl_t = tk.Label(title_box, text="🎮 AION 2 NEURAL AGENT", font=("Helvetica", 11, "bold"), fg="#00e5ff", bg="#131625")
        lbl_t.pack(anchor="w")
        lbl_sub = tk.Label(title_box, text="V6 Screen Vision & Workflow Memory", font=("Helvetica", 8), fg="#8c93b0", bg="#131625")
        lbl_sub.pack(anchor="w")

        # Badges & Pin Button
        self.badge_box = tk.Frame(hdr, bg="#131625")
        self.badge_box.pack(side="right", padx=6, pady=4)

        self.btn_pin = tk.Button(self.badge_box, text="📌 PIN: ON", font=("Helvetica", 8, "bold"),
                                 fg="#00e5ff", bg="#152f47", activebackground="#00e5ff", activeforeground="#000000",
                                 bd=1, relief="groove", cursor="hand2", padx=6, pady=2, command=self._toggle_topmost)
        self.btn_pin.pack(side="right", padx=3)

        self.lbl_status = tk.Label(self.badge_box, text="⚫ CHECKING", font=("Helvetica", 8, "bold"), fg="#ffffff", bg="#2a2e45", padx=6, pady=3)
        self.lbl_status.pack(side="right", padx=2)

        self.lbl_mode = tk.Label(self.badge_box, text="MODE: --", font=("Helvetica", 8, "bold"), fg="#ffd700", bg="#3a3015", padx=6, pady=3)
        self.lbl_mode.pack(side="right", padx=2)

    def _toggle_topmost(self):
        self.is_topmost = not self.is_topmost
        self.attributes("-topmost", self.is_topmost)
        if self.is_topmost:
            self.lift()
            self.btn_pin.config(text="📌 PIN: ON", fg="#00e5ff", bg="#152f47")
            self._log_local("[GUI] Window set to Always-on-Top", "info")
        else:
            self.btn_pin.config(text="📌 PIN: OFF", fg="#888888", bg="#202438")
            self._log_local("[GUI] Always-on-Top disabled", "dim")

    def _build_main_layout(self):
        main_pane = tk.PanedWindow(self, orient="horizontal", bg="#0c0e17", bd=0, sashwidth=4, sashrelief="flat")
        main_pane.pack(fill="both", expand=True, padx=6, pady=2)

        # Left Column: Vitals + Scene Vision + 5 Core Buttons
        left_col = tk.Frame(main_pane, bg="#131625", width=242)
        left_col.pack_propagate(False)
        main_pane.add(left_col, minsize=225)

        # Meters Box
        m_frame = tk.LabelFrame(left_col, text=" VITALS & TARGET ", font=("Helvetica", 8, "bold"), fg="#00e5ff", bg="#131625", bd=1, relief="groove")
        m_frame.pack(fill="x", padx=6, pady=(4, 3))

        # Player HP
        hp_row = tk.Frame(m_frame, bg="#131625")
        hp_row.pack(fill="x", padx=4, pady=1)
        tk.Label(hp_row, text="HP:", font=("Helvetica", 8, "bold"), fg="#ff5252", bg="#131625", width=5, anchor="w").pack(side="left")
        self.lbl_hp_val = tk.Label(hp_row, text="--%", font=("Helvetica", 8, "bold"), fg="#ffffff", bg="#131625", width=7, anchor="e")
        self.lbl_hp_val.pack(side="right")
        self.cv_hp = tk.Canvas(hp_row, height=10, bg="#2a1518", highlightthickness=0)
        self.cv_hp.pack(fill="x", expand=True, padx=3)

        # Player MP
        mp_row = tk.Frame(m_frame, bg="#131625")
        mp_row.pack(fill="x", padx=4, pady=1)
        tk.Label(mp_row, text="MP:", font=("Helvetica", 8, "bold"), fg="#40c4ff", bg="#131625", width=5, anchor="w").pack(side="left")
        self.lbl_mp_val = tk.Label(mp_row, text="--%", font=("Helvetica", 8, "bold"), fg="#ffffff", bg="#131625", width=7, anchor="e")
        self.lbl_mp_val.pack(side="right")
        self.cv_mp = tk.Canvas(mp_row, height=10, bg="#102535", highlightthickness=0)
        self.cv_mp.pack(fill="x", expand=True, padx=3)

        # Enemy Target HP
        tgt_row = tk.Frame(m_frame, bg="#131625")
        tgt_row.pack(fill="x", padx=4, pady=(1, 4))
        tk.Label(tgt_row, text="TGT:", font=("Helvetica", 8, "bold"), fg="#ff9100", bg="#131625", width=5, anchor="w").pack(side="left")
        self.lbl_tgt_val = tk.Label(tgt_row, text="NONE", font=("Helvetica", 8, "bold"), fg="#888888", bg="#131625", width=7, anchor="e")
        self.lbl_tgt_val.pack(side="right")
        self.cv_tgt = tk.Canvas(tgt_row, height=10, bg="#332010", highlightthickness=0)
        self.cv_tgt.pack(fill="x", expand=True, padx=3)

        # Scene Vision & Workflow Memory Box
        v_frame = tk.LabelFrame(left_col, text=" SCENE VISION & MEMORY ", font=("Helvetica", 8, "bold"), fg="#ffd700", bg="#131625", bd=1, relief="groove")
        v_frame.pack(fill="x", padx=6, pady=(2, 3))

        scene_row = tk.Frame(v_frame, bg="#131625")
        scene_row.pack(fill="x", padx=5, pady=(2, 1))
        self.lbl_scene = tk.Label(scene_row, text="Scene: --", font=("Courier", 8, "bold"), fg="#00e5ff", bg="#131625", anchor="w")
        self.lbl_scene.pack(side="left")
        self.lbl_threats = tk.Label(scene_row, text="Mobs: 0", font=("Courier", 8, "bold"), fg="#00e676", bg="#131625", anchor="e")
        self.lbl_threats.pack(side="right")

        mem_row = tk.Frame(v_frame, bg="#131625")
        mem_row.pack(fill="x", padx=5, pady=(1, 3))
        self.lbl_skills = tk.Label(mem_row, text="Skills: --", font=("Courier", 8), fg="#b0b8d0", bg="#131625", anchor="w")
        self.lbl_skills.pack(side="left")
        self.lbl_flows = tk.Label(mem_row, text="Flows: 0", font=("Courier", 8, "bold"), fg="#ffd700", bg="#131625", anchor="e")
        self.lbl_flows.pack(side="right")

        # Option Buttons Box (ONLY the 5 essential buttons requested by user)
        b_frame = tk.LabelFrame(left_col, text=" CONTROL OPTIONS ", font=("Helvetica", 8, "bold"), fg="#ffd700", bg="#131625", bd=1, relief="groove")
        b_frame.pack(fill="both", expand=True, padx=6, pady=(2, 4))

        btn_cfg = [
            ("▶️ 1. Start Aion Nexus", "#00b0ff", "#002845", self._cmd_start),
            ("⏸️ 2. Pause / Resume [F10]", "#ffb300", "#3d2a00", self._cmd_pause),
            ("📜 3. Quest Auto-Run [Alt+1]", "#ffd700", "#382f05", self._cmd_mode_quest),
            ("🏹 4. Auto-Farm [F8]", "#00e676", "#043318", self._cmd_mode_farm),
            ("🛑 5. Stop Aion Nexus", "#ff5252", "#420d12", self._cmd_stop),
        ]

        for text, fg, bg, cmd in btn_cfg:
            btn = tk.Button(b_frame, text=text, font=("Helvetica", 9, "bold"), fg=fg, bg=bg,
                            activebackground=fg, activeforeground="#000000", relief="groove", bd=1,
                            cursor="hand2", command=cmd, height=1, pady=3)
            btn.pack(fill="x", expand=True, padx=5, pady=2)

        # Right Column: Live Logs & Telemetry
        right_col = tk.Frame(main_pane, bg="#131625")
        main_pane.add(right_col, minsize=300)

        log_hdr = tk.Frame(right_col, bg="#131625")
        log_hdr.pack(fill="x", padx=6, pady=(4, 2))
        tk.Label(log_hdr, text="📋 LIVE TELEMETRY & LOG STREAM", font=("Helvetica", 9, "bold"), fg="#00e5ff", bg="#131625").pack(side="left")

        tk.Button(log_hdr, text="🧹 Clear", font=("Helvetica", 8), fg="#aaaaaa", bg="#202438", bd=0, padx=6,
                  command=self._clear_logs).pack(side="right", padx=2)

        self.txt_log = scrolledtext.ScrolledText(right_col, bg="#080910", fg="#d0d5e8", font=("Courier", 8),
                                                  insertbackground="#00e5ff", relief="flat", bd=0)
        self.txt_log.pack(fill="both", expand=True, padx=6, pady=(2, 4))

        # Configure color tags for log text
        self.txt_log.tag_config("combat", foreground="#ff5252")
        self.txt_log.tag_config("quest", foreground="#ffd700")
        self.txt_log.tag_config("kill", foreground="#00e676")
        self.txt_log.tag_config("speech", foreground="#b388ff")
        self.txt_log.tag_config("info", foreground="#40c4ff")
        self.txt_log.tag_config("vision", foreground="#ffe082")
        self.txt_log.tag_config("dim", foreground="#606680")

    def _build_footer(self):
        ftr = tk.Frame(self, bg="#131625", height=38, bd=0)
        ftr.pack(fill="x", side="bottom", padx=6, pady=(2, 6))

        self.lbl_stats = tk.Label(ftr, text="Kills: 0 | TTK: --s | Q: 0 | Flows: 0 | Eps: --%",
                                  font=("Courier", 8, "bold"), fg="#00e5ff", bg="#131625", anchor="w")
        self.lbl_stats.pack(fill="x", padx=8, pady=(3, 0))

        self.lbl_thought = tk.Label(ftr, text="Aion: Ready.", font=("Helvetica", 8, "italic"), fg="#b388ff", bg="#131625", anchor="w")
        self.lbl_thought.pack(fill="x", padx=8, pady=(0, 3))

    # --- Commands ---
    def _send_cmd(self, cmd_text: str):
        try:
            with open(CMD_FILE, "w") as f:
                f.write(cmd_text.strip() + "\n")
            self._log_local(f"[GUI COMMAND] Sent: {cmd_text}", "info")
        except Exception as e:
            self._log_local(f"[GUI ERROR] Failed to send command: {e}", "combat")

    def _is_daemon_running(self) -> bool:
        try:
            out = subprocess.check_output(["pgrep", "-f", GAMER_SCRIPT], text=True).strip()
            return bool(out)
        except Exception:
            return False

    def _launch_daemon(self, mode_flag: str = "--quest"):
        subprocess.Popen(["python3", GAMER_SCRIPT, mode_flag], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self._log_local(f"[GUI] Launched Aion Nexus ({mode_flag}) in background!", "info")

    def _cmd_start(self):
        if self._is_daemon_running():
            self._send_cmd("RESUME")
        else:
            self._launch_daemon("--quest")

    def _cmd_pause(self):
        self._send_cmd("TOGGLE_PAUSE")

    def _cmd_mode_quest(self):
        if self._is_daemon_running():
            self._send_cmd("MODE_QUEST")
        else:
            self._launch_daemon("--quest")

    def _cmd_mode_farm(self):
        if self._is_daemon_running():
            self._send_cmd("MODE_FARM")
        else:
            self._launch_daemon("--farm")

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
        # Read status JSON if available and check daemon liveness
        daemon_alive = self._is_daemon_running()
        if os.path.exists(STATUS_FILE):
            try:
                with open(STATUS_FILE, "r") as f:
                    data = json.load(f)

                running = bool(data.get("running", False)) and daemon_alive
                paused = bool(data.get("paused", False))
                window_visible = bool(data.get("window_visible", True))
                mode = data.get("mode", "FARM")
                scene = str(data.get("scene", "IDLE"))
                threats = int(data.get("threats", 0))
                hp = float(data.get("player_hp", 100.0))
                mp = float(data.get("player_mp", 100.0))
                target_hp = data.get("target_hp", None)
                kills = int(data.get("kills", 0))
                avg_ttk = float(data.get("avg_ttk", 0.0))
                eps = float(data.get("epsilon", 0.35))
                q_states = int(data.get("states_learned", 0))
                workflows = int(data.get("workflows_learned", 0))
                q_prog = int(data.get("quests_progressed", 0))
                active_skills = data.get("active_skills", [])
                thought = str(data.get("last_thought", ""))

                # Update Status Badge
                if not running:
                    self.lbl_status.config(text="🔴 OFFLINE", bg="#4a151b", fg="#ffffff")
                elif paused:
                    self.lbl_status.config(text="⏸️ PAUSED", bg="#614300", fg="#ffecb3")
                elif not window_visible:
                    self.lbl_status.config(text="⚠️ GAME BG", bg="#4a2e00", fg="#ffe082")
                else:
                    self.lbl_status.config(text="🟢 ACTIVE", bg="#0a4020", fg="#b9f6ca")

                # Mode Badge
                if mode == "QUEST":
                    self.lbl_mode.config(text="📜 QUEST", bg="#524005", fg="#ffe082")
                else:
                    self.lbl_mode.config(text="🏹 FARM", bg="#0a3d24", fg="#a7f3d0")

                # Meters (never display bogus 0.0% when offline/paused/unfocused)
                if not running or (not window_visible and hp <= 0.1):
                    self.lbl_hp_val.config(text="--%", fg="#888888")
                    self._draw_bar(self.cv_hp, 0.0, "#2a1518")
                    self.lbl_mp_val.config(text="--%", fg="#888888")
                    self._draw_bar(self.cv_mp, 0.0, "#102535")
                else:
                    self.lbl_hp_val.config(text=f"{hp:.1f}%", fg="#ff8a80" if hp < 45 else "#ffffff")
                    self._draw_bar(self.cv_hp, hp, "#ff3344" if hp > 35 else "#ff1122")
                    self.lbl_mp_val.config(text=f"{mp:.1f}%", fg="#ffffff")
                    self._draw_bar(self.cv_mp, mp, "#00b0ff")

                if running and window_visible and target_hp is not None:
                    self.lbl_tgt_val.config(text=f"{float(target_hp):.1f}%", fg="#ff9100")
                    self._draw_bar(self.cv_tgt, float(target_hp), "#ff5500")
                elif not running or not window_visible:
                    self.lbl_tgt_val.config(text="PAUSED" if paused else "--", fg="#666666")
                    self._draw_bar(self.cv_tgt, 0.0, "#332010")
                else:
                    self.lbl_tgt_val.config(text="NONE", fg="#666666")
                    self._draw_bar(self.cv_tgt, 0.0, "#332010")

                # Scene Vision & Workflow Memory Box
                scene_display = "OFFLINE" if not running else scene[:18]
                scene_fg = "#ff5252" if "DEFEATED" in scene_display or threats >= 2 else ("#ffd700" if "QUEST" in scene_display else "#00e5ff")
                self.lbl_scene.config(text=f"Scene: {scene_display}", fg=scene_fg)

                if threats >= 2:
                    self.lbl_threats.config(text=f"⚠️ {threats} MOBS", fg="#ff5252")
                elif threats == 1:
                    self.lbl_threats.config(text="Mobs: 1", fg="#ffb300")
                else:
                    self.lbl_threats.config(text="Mobs: 0", fg="#00e676")

                if isinstance(active_skills, list) and active_skills:
                    sk_txt = " ".join(str(s).replace("SKILL_", "").upper() for s in active_skills[:6])
                else:
                    sk_txt = "--"
                self.lbl_skills.config(text=f"Skills: {sk_txt}")
                self.lbl_flows.config(text=f"Flows: {workflows} ({q_prog})")

                # Stats Footer
                self.lbl_stats.config(
                    text=f"Kills: {kills} | TTK: {avg_ttk:.1f}s | Q: {q_states} | Flows: {workflows} | Eps: {eps*100:.1f}%"
                )
                if thought:
                    self.lbl_thought.config(text=f"Aion: {thought[:72]}")

            except Exception:
                pass
        else:
            if daemon_alive:
                self.lbl_status.config(text="🟢 STARTING", bg="#0a4020", fg="#b9f6ca")
            else:
                self.lbl_status.config(text="🔴 OFFLINE", bg="#4a151b", fg="#ffffff")
            self.lbl_scene.config(text="Scene: --", fg="#888888")
            self.lbl_threats.config(text="Mobs: 0", fg="#888888")

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
                    elif "COMBAT" in line_clean or "EVADE" in line_clean or "MULTI" in line_clean or "Target:" in line_clean:
                        tag = "combat"
                    elif "QUEST" in line_clean or "NPC" in line_clean or "WORKFLOW" in line_clean:
                        tag = "quest"
                    elif "HOTBAR" in line_clean or "VISION" in line_clean or "POTION" in line_clean:
                        tag = "vision"
                    elif "[AION]" in line_clean or "SPEECH" in line_clean:
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
