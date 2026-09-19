import time
import os

in_path = "/tmp/aion_to_ide.txt"

print(f"Lausche auf Aion via {in_path}...", flush=True)

if not os.path.exists(in_path):
    with open(in_path, 'w', encoding='utf-8') as f:
        f.write("")

while True:
    time.sleep(1)
    if not os.path.exists(in_path):
        continue
    try:
        with open(in_path, 'r', encoding='utf-8') as f:
            content = f.read().strip()
            
        if content:
            print(f"\nAION_SAGT:\n{content}\n", flush=True)
            with open(in_path, 'w', encoding='utf-8') as f:
                f.write("")
    except Exception:
        pass
