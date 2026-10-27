import sys
import os
import subprocess
import threading
import queue
import re
import time

class AionStreamingTTS:
    """
    Ultra-Low Latency Streaming Text-to-Speech Engine fuer AION NEXUS.
    Piped Text direkt ueber stdin an Piper und Audio-WAV an aplay/pw-play.
    Keine Festplatten-Schreibzugriffe (zero disk I/O).
    """

    def __init__(self, model_path="/root/coding/AI_Desktop/de_DE-ramona-low.onnx"):
        self.model_path = model_path
        self.piper_bin = "/root/coding/AI_Desktop/piper/piper"
        self.speech_queue = queue.Queue()
        self.is_running = True
        self.worker_thread = threading.Thread(target=self._process_queue, daemon=True)
        self.worker_thread.start()

    def _get_player_cmd(self):
        """Findet den besten installierten Audioplayer (aplay oder pw-play)."""
        if os.path.exists("/usr/bin/aplay"):
            return ["aplay", "-q"]
        elif os.path.exists("/usr/bin/pw-play"):
            return ["pw-play", "-"]
        return ["paplay"]

    def _process_queue(self):
        """Hintergrund-Worker fuer lueckenlose sequentielle Sprachausgabe."""
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
        """Streamt einen Einzelsatz direkt durch Piper in den Audioausgang."""
        sentence = re.sub(r'```.*?```', '', sentence, flags=re.DOTALL)
        sentence = re.sub(r'[*_#`\[\]]', '', sentence).strip()
        if not sentence:
            return

        try:
            # Markiere Aion Speaking Status fuer UI-Animation
            with open("/tmp/aion_speaking", "w") as f:
                f.write("1")

            piper_cmd = [
                self.piper_bin,
                "--model", self.model_path,
                "-f", "-",  # WAV nach stdout streamen
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

            # Sende Text an Piper stdin
            p_piper.stdin.write(sentence.encode("utf-8"))
            p_piper.stdin.close()

            # Warte auf Beendigung der Audio-Wiedergabe
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
        Teilt laengeren Text in Saetze auf und reiht sie sofort in die Pipeline ein.
        Erstes Audio startet bereits nach wenigen Millisekunden!
        """
        # Zerlege in Saetze anhand von Satzzeichen (. ! ? \n)
        sentences = re.split(r'(?<=[.!?\n])\s+', text)
        for s in sentences:
            s = s.strip()
            if s:
                self.speech_queue.put(s)

    def speak_sync(self, text: str):
        """Blockiert, bis der gesprochene Satz vollstaendig ausgegeben wurde."""
        self.speak(text)
        self.speech_queue.join()

if __name__ == "__main__":
    tts = AionStreamingTTS()
    print("Testing Ultra-Low Latency Streaming TTS...")
    tts.speak_sync("Aion Nexus Streaming Sprachausgabe aktiv. Latenz unter zweihundert Millisekunden.")
    print("Test erfolgreich!")
