import sys
import time
import json
import os
import re

if len(sys.argv) < 2:
    print("Error: Please provide the conversation ID as the first argument.")
    sys.exit(1)

conversation_id = sys.argv[1]
transcript_path = f"/root/.gemini/antigravity-ide/brain/{conversation_id}/.system_generated/logs/transcript.jsonl"
out_path = "/tmp/aion_ide_out.txt"

def strip_markdown(text):
    # Very basic markdown stripping
    text = re.sub(r'```.*?```', '', text, flags=re.DOTALL)
    text = re.sub(r'\*\*(.*?)\*\*', r'\1', text)
    text = re.sub(r'\*(.*?)\*', r'\1', text)
    text = re.sub(r'\[(.*?)\]\(.*?\)', r'\1', text)
    text = re.sub(r'<.*?>', '', text)
    return text.strip()

last_size = 0
if os.path.exists(transcript_path):
    last_size = os.path.getsize(transcript_path)

print(f"Monitoring {transcript_path} for IDE responses...")
while True:
    time.sleep(1)
    if not os.path.exists(transcript_path):
        continue
    
    current_size = os.path.getsize(transcript_path)
    if current_size > last_size:
        with open(transcript_path, 'r', encoding='utf-8') as f:
            f.seek(last_size)
            new_data = f.read()
            last_size = current_size
            
            for line in new_data.strip().split('\n'):
                if not line:
                    continue
                try:
                    entry = json.loads(line)
                    if entry.get("type") == "PLANNER_RESPONSE":
                        content = entry.get("content", "")
                        if content:
                            clean_text = strip_markdown(content)
                            if "[AION]" in clean_text:
                                aion_msg = clean_text.split("[AION]")[1].strip()
                                if aion_msg:
                                    with open('/tmp/ide_speech.txt', 'w', encoding='utf-8') as sf:
                                        sf.write(aion_msg)
                                    print("Playing IDE TTS...")
                                    os.system('nice -n 19 /root/coding/AI_Desktop/piper/piper --model /root/coding/AI_Desktop/de_DE-ramona-low.onnx --output_file /tmp/ide_speech.wav < /tmp/ide_speech.txt && pw-play /tmp/ide_speech.wav')
                                    
                                    with open(out_path, 'w', encoding='utf-8') as out:
                                        out.write(aion_msg)
                                    print(f"Sent to Aion: {aion_msg[:50]}...")
                except Exception as e:
                    pass
