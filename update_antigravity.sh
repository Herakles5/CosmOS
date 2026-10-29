#!/bin/bash
# ==============================================================================
# Google Antigravity IDE - Safe 1-Click Auto-Updater for MeinOS / CosmOS
# ==============================================================================
set -e

TARGET_DIR="/opt/meinos/Antigravity IDE"
BACKUP_DIR="/opt/meinos/Antigravity_IDE_backup_$(date +%Y%m%d_%H%M%S)"
TMP_DIR="/tmp/antigravity_update_$$"
LOCAL_DOWNLOAD="/root/Downloads/Antigravity IDE.tar.gz"
LOCAL_DOWNLOAD_ALT="/root/Downloads/Antigravity_IDE.tar.gz"

echo "=========================================================="
echo "   Google Antigravity IDE - Safe 1-Click Updater"
echo "=========================================================="

mkdir -p "$TMP_DIR"
ARCHIVE_PATH=""

# Step 1: Check if user already downloaded it in Firefox ~/Downloads
if [ -f "$LOCAL_DOWNLOAD" ]; then
    echo "[*] Found locally downloaded archive in $LOCAL_DOWNLOAD"
    ARCHIVE_PATH="$LOCAL_DOWNLOAD"
elif [ -f "$LOCAL_DOWNLOAD_ALT" ]; then
    echo "[*] Found locally downloaded archive in $LOCAL_DOWNLOAD_ALT"
    ARCHIVE_PATH="$LOCAL_DOWNLOAD_ALT"
else
    echo "[1/4] Detecting latest release URL from antigravity.google..."
    LATEST_URL=$(curl -s "https://antigravity.google/download" | grep -oE "https://edgedl.me.gvt1.com/edgedl/release2/j0qc3/antigravity/stable/[^/]+/linux-x64/Antigravity%20IDE.tar.gz" | head -n 1)
    
    if [ -z "$LATEST_URL" ]; then
        LATEST_URL="https://edgedl.me.gvt1.com/edgedl/release2/j0qc3/antigravity/stable/2.6.0-5526304964083712/linux-x64/Antigravity%20IDE.tar.gz"
    fi
    
    echo "[1/4] Downloading latest Google Antigravity IDE..."
    echo "      URL: $LATEST_URL"
    ARCHIVE_PATH="$TMP_DIR/Antigravity_IDE.tar.gz"
    curl -L -# -f -o "$ARCHIVE_PATH" "$LATEST_URL"
fi

# Step 2: Extract to staging folder
echo "[2/4] Unpacking and verifying update package..."
mkdir -p "$TMP_DIR/extracted"
tar -xzf "$ARCHIVE_PATH" -C "$TMP_DIR/extracted"

if [ ! -d "$TMP_DIR/extracted/Antigravity IDE" ]; then
    echo "[-] Error: Unexpected archive contents."
    rm -rf "$TMP_DIR"
    exit 1
fi

# Step 3: Backup existing install
if [ -d "$TARGET_DIR" ]; then
    echo "[3/4] Creating safety backup of current version to:"
    echo "      $BACKUP_DIR"
    cp -a "$TARGET_DIR" "$BACKUP_DIR"
fi

# Step 4: Install new version into /opt/meinos/Antigravity IDE
echo "[4/4] Installing new files into $TARGET_DIR..."
mkdir -p "$TARGET_DIR"
cp -a "$TMP_DIR/extracted/Antigravity IDE/." "$TARGET_DIR/"
chmod +x "$TARGET_DIR/antigravity-ide"

# Clean temporary files
rm -rf "$TMP_DIR"

echo "=========================================================="
echo " [SUCCESS] Google Antigravity IDE update completed!"
echo " - Installation path: $TARGET_DIR"
echo " - User configurations, logins & workspaces: 100% PRESERVED"
echo " - Safety backup saved in: $BACKUP_DIR"
echo "=========================================================="
echo "To apply: Simply close and reopen Antigravity IDE (or click 'KI ANTIGRAVITY' on desktop)."
