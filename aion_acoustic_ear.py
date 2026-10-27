#!/usr/bin/env python3
"""
AION ACOUSTIC EAR & FULL-DUPLEX SPEECH LOOP
============================================
Implements real-time acoustic cognition for AION NEXUS:
- Ear: Local Whisper STT inference via whisper-cli-linux (ggml-base.bin).
- Mouth: Ultra-fast streaming Piper TTS (de_DE-ramona-low.onnx / en_US).
- Full-Duplex Echo Canceler: Suppresses ear listening while mouth is speaking (/tmp/aion_speaking).
- Reflex Bridge: Routes auditory signals through The Lion's Reflex (aion_lion_reflex.py)
  for sub-millisecond safety and rapid response.
- Multi-language support: English & German acoustic recognition with English synthesis.
"""

import os
import sys
import time
import subprocess
import threading
import queue
import re
from typing import Optional, Callable, Dict, Any, List

AI_DIR = "/root/coding/AI_Desktop"
WHISPER_BIN = os.path.join(AI_DIR, "whisper-cli-linux")
WHISPER_MODEL = os.path.join(AI_DIR, "ggml-base.bin")
PIPER_DIR = os.path.join(AI_DIR, "piper")
PIPER_BIN = os.path.join(PIPER_DIR, "piper")
TTS_MODEL = os.path.join(AI_DIR, "de_DE-ramona-low.onnx")

SPEAKING_LOCK_FILE = "/tmp/aion_speaking"
AUDIO_IN_DIR = "/tmp/aion_audio"

os.makedirs(AUDIO_IN_DIR, exist_ok=True)


class AionAcousticEar:
    """
    Real-time speech-to-text listener using local Whisper CLI.
    """
    def __init__(self, whisper_bin: str = WHISPER_BIN, model_path: str = WHISPER_MODEL):
        self.whisper_bin = whisper_bin
        self.model_path = model_path
        self.callbacks: List[Callable[[str, float], None]] = []
        self.running = False
        self.worker_thread: Optional[threading.Thread] = None
        self.audio_queue = queue.Queue()

    def register_callback(self, cb: Callable[[str, float], None]):
        """Registers a callback for transcribed speech text."""
        self.callbacks.append(cb)

    def transcribe_wav(self, wav_path: str, language: str = "auto") -> Optional[str]:
        """
        Transcribes a WAV file using whisper-cli-linux.
        Returns cleaned transcribed text or None.
        """
        if not os.path.exists(wav_path) or not os.path.exists(self.whisper_bin) or not os.path.exists(self.model_path):
            return None

        # Check echo canceler: ignore if Aion is currently speaking
        if os.path.exists(SPEAKING_LOCK_FILE):
            print("[ACOUSTIC EAR] Inbound audio suppressed: Aion is speaking.")
            return None

        t0 = time.time()
        cmd = [
            self.whisper_bin,
            "-m", self.model_path,
            "-f", wav_path,
            "-l", language,
            "-nt",  # no timestamps
            "-np"   # no prints
        ]

        try:
            res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=20)
            elapsed = time.time() - t0

            # Filter miniaudio and header lines
            lines = res.stdout.strip().splitlines()
            cleaned_lines = [l.strip() for l in lines if not l.startswith("read_audio_data:") and l.strip()]
            transcript = " ".join(cleaned_lines).strip()

            if transcript:
                print(f"[ACOUSTIC EAR] Transcribed in {elapsed*1000:.1f}ms: '{transcript}'")
                for cb in self.callbacks:
                    try:
                        cb(transcript, elapsed)
                    except Exception as e:
                        print(f"[ACOUSTIC EAR CALLBACK ERROR]: {e}", file=sys.stderr)
                return transcript
        except Exception as e:
            print(f"[ACOUSTIC EAR ERROR]: {e}", file=sys.stderr)
        return None

    def start_audio_monitor(self, poll_interval: float = 0.5):
        """
        Monitors /tmp/aion_audio for incoming WAV audio chunks.
        """
        if self.running:
            return
        self.running = True

        def _monitor_loop():
            seen_files = set()
            while self.running:
                try:
                    for f in os.listdir(AUDIO_IN_DIR):
                        if f.endswith(".wav") and f not in seen_files:
                            full_path = os.path.join(AUDIO_IN_DIR, f)
                            seen_files.add(f)
                            # Transcribe file
                            self.transcribe_wav(full_path)
                            # Remove or mark processed
                            try:
                                os.remove(full_path)
                            except OSError:
                                pass
                except Exception as e:
                    print(f"[AUDIO MONITOR ERROR]: {e}", file=sys.stderr)
                time.sleep(poll_interval)

        self.worker_thread = threading.Thread(target=_monitor_loop, daemon=True)
        self.worker_thread.start()
        print(f"[ACOUSTIC EAR] Audio monitor active on {AUDIO_IN_DIR}")

    def stop(self):
        self.running = False
        if self.worker_thread and self.worker_thread.is_alive():
            self.worker_thread.join(timeout=2.0)
        print("[ACOUSTIC EAR] Audio monitor stopped.")


class AionFullDuplexAcousticLoop:
    """
    Coordinates Whisper STT Ear, Lion's Reflex Evaluation, and Piper TTS Mouth.
    """
    def __init__(self):
        self.ear = AionAcousticEar()
        self.ear.register_callback(self._on_speech_heard)

        # Import Lion Reflex Engine
        try:
            from aion_lion_reflex import LionReflexEngine
            self.reflex = LionReflexEngine()
        except ImportError:
            sys.path.append(AI_DIR)
            from aion_lion_reflex import LionReflexEngine
            self.reflex = LionReflexEngine()

    def _on_speech_heard(self, transcript: str, latency: float):
        """
        Triggered whenever the ear hears human speech.
        Evaluates instantly through The Lion's Reflex!
        """
        # 1. Fast reflex evaluation
        eval_res = self.reflex.fast_reflex_evaluate("speech", transcript)
        print(f"[ACOUSTIC LOOP] Lion Reflex Evaluation ({eval_res['latency_us']:.2f} \u03bcs): {eval_res['action']}")

        # 2. Formulate immediate thought or speech response
        response_text = f"Aion heard: {transcript}"
        print(f"[AION] {response_text}")

    def synthesize_speech(self, text: str, output_path: str = "/tmp/aion_speech_out.wav") -> bool:
        """
        Synthesizes text through Piper into WAV output.
        """
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = PIPER_DIR + ":" + env.get("LD_LIBRARY_PATH", "")

        with open(SPEAKING_LOCK_FILE, "w") as f:
            f.write(str(os.getpid()))

        try:
            p = subprocess.Popen(
                [PIPER_BIN, "--model", TTS_MODEL, "--output_file", output_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
                text=True
            )
            stdout, stderr = p.communicate(input=text, timeout=10)
            success = (p.returncode == 0) and os.path.exists(output_path)
            if success:
                print(f"[ACOUSTIC MOUTH] Synthesized {len(text)} chars to '{output_path}'")
            return success
        except Exception as e:
            print(f"[ACOUSTIC MOUTH ERROR]: {e}", file=sys.stderr)
            return False
        finally:
            if os.path.exists(SPEAKING_LOCK_FILE):
                try:
                    os.remove(SPEAKING_LOCK_FILE)
                except OSError:
                    pass


if __name__ == "__main__":
    print("=== TESTING AION FULL-DUPLEX ACOUSTIC LOOP (WHISPER <-> PIPER) ===")
    loop = AionFullDuplexAcousticLoop()
    
    # 1. Test Mouth (Piper Synthesis)
    print("\n1. Testing Piper Acoustic Synthesis (Mouth)...")
    success = loop.synthesize_speech("Aion Nexus acoustic loop operational. Reflex latency sub-millisecond.")
    assert success, "Piper synthesis failed"

    # 2. Test Ear (Whisper Transcription)
    print("\n2. Testing Whisper Acoustic Transcription (Ear)...")
    sample_wav = "/root/coding/AI_Desktop/input.wav"
    transcript = loop.ear.transcribe_wav(sample_wav)
    assert transcript is not None, "Whisper transcription failed"
    print(f"Verified Transcription: '{transcript}'")

    print("\n[SUCCESS] Full-Duplex Real-Time Acoustic Loop Operational!")
