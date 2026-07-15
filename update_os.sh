#!/bin/bash
# Skript zum Aktualisieren des Betriebssystems auf dem Laufenden System

SOURCE_FILE="./meinos_v2.elf"
DEST_FILE="/opt/meinos/meinos.elf"

if [ "$EUID" -ne 0 ]; then
  echo "Bitte als root ausführen (sudo ./update_os.sh)"
  exit 1
fi

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Fehler: $SOURCE_FILE wurde nicht gefunden."
    echo "Hast du den Code mit 'make' kompiliert?"
    exit 1
fi

echo "Kopiere $SOURCE_FILE nach $DEST_FILE..."
rm -f "$DEST_FILE"
cp "$SOURCE_FILE" "$DEST_FILE"
chmod +x "$DEST_FILE"

echo "Update erfolgreich abgeschlossen! Bitte rebooten."
