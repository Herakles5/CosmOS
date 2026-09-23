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
    # Remove file links and technical artifacts
    text = re.sub(r'file:///[^\s)]+', '', text)
    return text.strip()

def get_aion_lang():
    """Read the aion_lang setting from the shared config file, fallback to checking /tmp"""
    # Check for the language setting file written by aion_app.cpp
    try:
        with open('/tmp/aion_lang.txt', 'r') as f:
            lang = f.read().strip()
            if lang == '0':
                return 'en'
            elif lang == '1':
                return 'de'
            elif lang == '2':
                return 'ko'
    except:
        pass
    return 'de'  # Default to German

def speak_text(text):
    """Speak text using Piper TTS with the correct language model"""
    if not text or len(text.strip()) < 3:
        return
    
    lang = get_aion_lang()
    if lang == 'de':
        piper_model = "/root/coding/AI_Desktop/de_DE-ramona-low.onnx"
    else:
        piper_model = "/root/coding/AI_Desktop/voice.onnx"
    
    with open('/tmp/ide_speech.txt', 'w', encoding='utf-8') as sf:
        sf.write(text)
    
    print(f"Playing IDE TTS (lang={lang})...")
    os.system(f'nice -n 19 /root/coding/AI_Desktop/piper/piper --model {piper_model} --output_file /tmp/ide_speech.wav < /tmp/ide_speech.txt && pw-play /tmp/ide_speech.wav')

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
                            if not clean_text or len(clean_text) < 5:
                                continue
                            
                            # Truncate very long responses for TTS (max ~500 chars)
                            speech_text = clean_text
                            if len(speech_text) > 500:
                                speech_text = speech_text[:500] + "..."
                            
                            # Send to AION display
                            with open(out_path, 'w', encoding='utf-8') as out:
                                out.write(clean_text)
                            print(f"Sent to Aion: {clean_text[:50]}...")
                            
                            # Speak the response
                            speak_text(speech_text)
                except Exception as e:
                    pass
