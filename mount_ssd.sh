#!/bin/bash
# Skript zum Mounten der SSD (sdc1)

MOUNT_DIR="/mnt/ssd"

if [ "$EUID" -ne 0 ]; then
  echo "Bitte als root ausführen (sudo ./mount_ssd.sh)"
  exit 1
fi

mkdir -p "$MOUNT_DIR"

echo "Mounte /dev/sdc1 nach $MOUNT_DIR..."
mount /dev/sdc1 "$MOUNT_DIR"

if [ $? -eq 0 ]; then
    echo "Erfolgreich gemountet unter $MOUNT_DIR"
else
    echo "Fehler beim Mounten von /dev/sdc1"
fi
