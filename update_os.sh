#!/bin/bash
# Script to update the operating system binary on the running host system

SOURCE_FILE="./meinos_v2.elf"
DEST_FILE="/opt/meinos/meinos.elf"

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo ./update_os.sh)"
  exit 1
fi

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: $SOURCE_FILE not found."
    echo "Did you compile the binary with 'make -f Makefile.linux'?"
    exit 1
fi

echo "Copying $SOURCE_FILE to $DEST_FILE..."
rm -f "$DEST_FILE"
cp "$SOURCE_FILE" "$DEST_FILE"
chmod +x "$DEST_FILE"
cp face.png /opt/meinos/face.png || true

echo "Update completed successfully! Please reboot."
