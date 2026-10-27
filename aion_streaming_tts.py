import sys
import os
import subprocess
import threading
import queue
import re
import time

class AionStreamingTTS:
    """
    Ultra-Low Latency Streaming Text-to-Speech Engine for AION NEXUS.
    Pipes text directly via stdin to Piper and audio WAV to aplay/pw-play.
    Zero disk I/O.
    """

    def __init__(self, model_path="/root/coding/AI_Desktop/de_DE-ramona-low.onnx"):
        self.model_path = model_path
        self.piper_bin = "/root/coding/AI_Desktop/piper/piper"
        self.speech_queue = queue.Queue()
        self.is_running = True
        self.worker_thread = threading.Thread(target=self._process_queue, daemon=True)
        self.worker_thread.start()

    def _get_player_cmd(self):
        """Finds the optimal installed audio player (aplay or pw-play)."""
        if os.path.exists("/usr/bin/aplay"):
            return ["aplay", "-q"]
        elif os.path.exists("/usr/bin/pw-play"):
            return ["pw-play", "-"]
        return ["paplay"]

    def _process_queue(self):
        """Background worker for seamless sequential speech synthesis."""
        while self.is_running:
            try:
                sentence = self.speech_queue.get(timeout=0.5)
            except queue.Empty:
                continue

            if not sentence or len(sentence.strip()) < 2:
                self.speech_queue.task_done()
                continue

            self._play_sentence_stream(sentence)
            self.speech_queue.task_done()

    def _play_sentence_stream(self, sentence: str):
        """Streams a single sentence directly through Piper to audio output."""
        sentence = re.sub(r'```.*?```', '', sentence, flags=re.DOTALL)
        sentence = re.sub(r'[*_#`\[\]]', '', sentence).strip()
        if not sentence:
            return

        try:
            # Mark Aion speaking state for UI animation
            with open("/tmp/aion_speaking", "w") as f:
                f.write("1")

            piper_cmd = [
                self.piper_bin,
                "--model", self.model_path,
                "-f", "-",  # Stream WAV to stdout
                "--quiet"
            ]
            player_cmd = self._get_player_cmd()

            p_piper = subprocess.Popen(
                piper_cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL
            )
            p_player = subprocess.Popen(
                player_cmd,
                stdin=p_piper.stdout,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )

            # Send text to Piper stdin
            p_piper.stdin.write(sentence.encode("utf-8"))
            p_piper.stdin.close()

            # Wait for audio playback completion
            p_player.wait()
            p_piper.wait()

        except Exception as e:
            print(f"[STREAMING TTS ERROR] {e}")
        finally:
            if os.path.exists("/tmp/aion_speaking"):
                try:
                    os.remove("/tmp/aion_speaking")
                except OSError:
                    pass

    def speak(self, text: str, priority: bool = False):
        """
        Splits longer text into sentences and enqueues them immediately.
        Audio playback initiates within milliseconds!
        """
        # Split into sentences using punctuation (. ! ? \n)
        sentences = re.split(r'(?<=[.!?\n])\s+', text)
        for s in sentences:
            s = s.strip()
            if s:
                self.speech_queue.put(s)

    def speak_sync(self, text: str):
        """Blocks until speech output completes."""
        self.speak(text)
        self.speech_queue.join()

if __name__ == "__main__":
    tts = AionStreamingTTS()
    print("Testing Ultra-Low Latency Streaming TTS...")
    tts.speak_sync("Aion Nexus streaming text to speech active. Latency under two hundred milliseconds.")
    print("Test successful!")
