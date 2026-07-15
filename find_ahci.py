import os
for root, dirs, files in os.walk("."):
    for file in files:
        if file.endswith(".cpp") or file.endswith(".S"):
            with open(os.path.join(root, file), "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
                if "ahci_read_sectors(int" in content or "ahci_read_sectors(uint32_t port" in content:
                    print(f"Found in {file}")
