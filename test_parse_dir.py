import os
import struct

for file in os.listdir("."):
    if file.endswith(".exe"):
        print(f"File: {file}")
