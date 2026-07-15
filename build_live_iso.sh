#!/bin/bash
set -e

clear
echo "=========================================="
echo "    CosmOS Build-System"
echo "=========================================="
echo "Welche ISO-Version möchtest du bauen?"
echo " [1] FAST MODE    (Gzip: Baut in Sekunden, ca. 1.2 GB, für schnelle Tests)"
echo " [2] RELEASE MODE (XZ Max: Dauert sehr lange, kleinste Datei für Hardware)"
read -t 5 -p "Wähle 1 oder 2 (Standard ist 1 nach 5 Sekunden): " BUILD_MODE || BUILD_MODE="1"
if [ -z "$BUILD_MODE" ]; then BUILD_MODE="1"; fi

if [ "$BUILD_MODE" == "2" ]; then
    # Extrem starke Komprimierung mit 1MB Blockgröße (Maximales Limit). Speicher limitiert, um Abstürze zu vermeiden!
    COMP_ARGS="-comp xz -b 1M -Xbcj x86 -mem 2G"
    echo "-> RELEASE MODE aktiviert. Hol dir einen Kaffee, das dauert jetzt..."
    sleep 2
else
    # Rasend schnelle Komprimierung
    COMP_ARGS="-comp gzip"
    echo "-> FAST MODE aktiviert. Turbogang eingelegt!"
    sleep 1
fi

echo "------------------------------------------"
echo "-> Erstelle automatisches Backup..."
BACKUP_DIR="backups/cosmos_$(date +%Y-%m-%d_%H-%M-%S)"
mkdir -p "$BACKUP_DIR"
cp *.cpp *.h *.s *.ld "$BACKUP_DIR/" 2>/dev/null || true
echo "-> Quellcodes sicher kopiert nach: $BACKUP_DIR"

echo "------------------------------------------"
echo "0. Alten Müll aufräumen..."

echo "1. Cleaning up old build files..."
rm -f CosmOS.iso

# FIX: /tmp mit dev+exec remounten, damit debootstrap Device-Nodes erstellen kann!
mount -o remount,dev,exec /tmp 2>/dev/null || true

# WICHTIG: Kompletten alten Datenmüll aus dem RAM-Disk löschen, damit er nicht bei 97% vollläuft!
rm -rf /tmp/meinos_build
mkdir -p /tmp/meinos_build

echo "2. Compiling meinos.elf (Linux version)..."
make -f Makefile.linux clean
make -f Makefile.linux

echo "3. Creating minimal root filesystem (This will take a while)..."
export ROOTFS=/tmp/meinos_build/live_rootfs
mkdir -p $ROOTFS
if [ ! -f "$ROOTFS/bin/bash" ]; then
    debootstrap --variant=minbase --arch=amd64 noble $ROOTFS http://archive.ubuntu.com/ubuntu/
else
    echo "Base system already extracted, skipping debootstrap."
fi

echo "4. Configuring the root filesystem..."
mount -t proc none $ROOTFS/proc || true
mount -t sysfs none $ROOTFS/sys || true
mount -o bind /dev $ROOTFS/dev || true
mount -o bind /dev/pts $ROOTFS/dev/pts || true

# Install necessary packages inside chroot
chroot $ROOTFS /bin/bash << 'EOF'
export DEBIAN_FRONTEND=noninteractive
echo 'deb http://archive.ubuntu.com/ubuntu/ noble universe' > /etc/apt/sources.list.d/universe.list
apt-get update

# === COSMOS BASIS & INSTALLER PAKETE ===
apt-get install -y --no-install-recommends \
    linux-image-generic live-boot systemd systemd-sysv alsa-utils pipewire pipewire-pulse \
    xserver-xorg xserver-xorg-video-all xserver-xorg-input-all xinit \
    libsdl2-2.0-0 libgl1-mesa-dri libglx-mesa0 x11-xserver-utils ifupdown linux-firmware \
    xterm rsync os-prober parted sudo dosfstools ntfs-3g efibootmgr network-manager blueman xfce4-screenshooter \
    openbox grub-pc-bin grub-efi-amd64-bin grub2-common wpasupplicant \
    gparted pcmanfm xfce4-terminal mousepad xfce4-appfinder mpv wget ca-certificates \
    exfatprogs \
    psmisc \
    xdg-utils \
    libnss3 libasound2t64 libatk-bridge2.0-0 libgtk-3-0 libgbm1 libxkbcommon0 \
    build-essential \
    libsdl2-dev libgl-dev \
    ffmpeg wireplumber dbus-x11 \
    debootstrap squashfs-tools xorriso mtools \
    udisks2 gvfs gvfs-backends polkitd

# === FIREFOX (OHNE SNAP VIA MOZILLA PPA) ===
install -d -m 0755 /etc/apt/keyrings
wget -q https://packages.mozilla.org/apt/repo-signing-key.gpg -O- > /etc/apt/keyrings/packages.mozilla.org.asc
echo 'deb [signed-by=/etc/apt/keyrings/packages.mozilla.org.asc] https://packages.mozilla.org/apt mozilla main' > /etc/apt/sources.list.d/mozilla.list
echo 'Package: *
Pin: origin packages.mozilla.org
Pin-Priority: 1000' > /etc/apt/preferences.d/mozilla
apt-get update
apt-get install -y firefox

# === GOOGLE ANTIGRAVITY (KI ASSISTANT) DIREKT IN DIE ISO BACKEN ===
echo "-> Lade KI-Agenten herunter..."
# URL existiert nicht, daher vorerst deaktiviert, um Fehler zu vermeiden.
# wget -qO /tmp/antigravity.deb https://dl.google.com/linux/direct/google-antigravity-stable_current_amd64.deb
# echo "-> Installiere KI-Agenten..."
# dpkg -i /tmp/antigravity.deb || apt-get install -f -y
# rm -f /tmp/antigravity.deb

apt-get clean
# PROFI-TRICK: Löscht den ungenutzten Paket-Cache. Spart riesige Mengen an Platz in der finalen ISO!
rm -rf /var/lib/apt/lists/*
rm -rf /tmp/*
EOF

# === NETZWERK-KONFIGURATION (LAN + WiFi + Bluetooth) ===
echo "4.5 Configuring Networking (LAN + WiFi + BT)..."

# Nur loopback in interfaces, alles andere managt NetworkManager!
mkdir -p $ROOTFS/etc/network
echo "auto lo" > $ROOTFS/etc/network/interfaces
echo "iface lo inet loopback" >> $ROOTFS/etc/network/interfaces

# NetworkManager fuer ALLES (LAN, WiFi, BT) aktivieren!
# Alle anderen Netzwerk-Dienste ABSCHALTEN damit nichts kollidiert!
chroot $ROOTFS /usr/bin/systemctl enable NetworkManager || true
chroot $ROOTFS /usr/bin/systemctl disable systemd-networkd || true
chroot $ROOTFS /usr/bin/systemctl disable systemd-resolved || true
chroot $ROOTFS /usr/bin/systemctl mask systemd-networkd || true
chroot $ROOTFS /usr/bin/systemctl mask systemd-resolved || true
chroot $ROOTFS /usr/bin/systemctl mask systemd-networkd-wait-online.service || true

# Loesche alte systemd-networkd Reste
rm -rf $ROOTFS/etc/systemd/network/*

# NetworkManager: ALLE Interfaces verwalten (nicht nur WiFi)!
mkdir -p $ROOTFS/etc/NetworkManager/conf.d
cat > $ROOTFS/etc/NetworkManager/conf.d/10-manage-all.conf << 'NMEOF'
[keyfile]
unmanaged-devices=none

[connection]
ethernet.cloned-mac-address=permanent
wifi.cloned-mac-address=permanent

[device]
wifi.scan-rand-mac-address=no
NMEOF

# DHCP: FritzBox-kompatibel! Immer die echte MAC-Adresse senden!
mkdir -p $ROOTFS/etc/NetworkManager/conf.d
cat > $ROOTFS/etc/NetworkManager/conf.d/20-dhcp-mac.conf << 'DHCPEOF'
[main]
dhcp=internal

[ipv4]
dhcp-client-id=mac
method=auto
DHCPEOF

# DNS: Hardcoded Google DNS als Fallback, falls der Router keinen DNS liefert!
rm -f $ROOTFS/etc/resolv.conf
echo "nameserver 8.8.8.8" > $ROOTFS/etc/resolv.conf
echo "nameserver 1.1.1.1" >> $ROOTFS/etc/resolv.conf

# FIX: Prevent background updates from freezing the Live CD
rm -f $ROOTFS/lib/systemd/system/apt-daily.timer
rm -f $ROOTFS/lib/systemd/system/apt-daily.service
rm -f $ROOTFS/lib/systemd/system/apt-daily-upgrade.timer
rm -f $ROOTFS/lib/systemd/system/apt-daily-upgrade.service


echo "5. Setting up Auto-Boot into MeinOS..."
mkdir -p $ROOTFS/opt/meinos
cp meinos_v2.elf $ROOTFS/opt/meinos/meinos.elf
# APP.BIN als Trigger-Datei anlegen (Inhalt egal - die Engine ist in meinos.elf einkompiliert)
echo "MEINOS_APP_TRIGGER" > $ROOTFS/opt/meinos/APP.BIN
cp ROMS.tba $ROOTFS/opt/meinos/ || true
cp -r sounds $ROOTFS/opt/meinos/ || true
cp install_cosmos.sh $ROOTFS/opt/meinos/ || true
chmod +x $ROOTFS/opt/meinos/install_cosmos.sh || true
cp -r "Antigravity IDE" $ROOTFS/opt/meinos/ || true

cat > $ROOTFS/root/.xinitrc << 'EOF'
#!/bin/sh
xset -dpms
xset s off
xset s noblank

# Openbox Keybinding: DRUCK-Taste = Screenshot (systemweit!)
mkdir -p $ROOTFS/root/.config/openbox
cp $ROOTFS/etc/xdg/openbox/rc.xml $ROOTFS/root/.config/openbox/rc.xml
# Ersetze die Standard "scrot" Befehle durch "xfce4-screenshooter"
sed -i 's/<command>scrot -s<\/command>/<command>xfce4-screenshooter -w<\/command>/g' $ROOTFS/root/.config/openbox/rc.xml
sed -i 's/<command>scrot<\/command>/<command>xfce4-screenshooter -f<\/command>/g' $ROOTFS/root/.config/openbox/rc.xml

# Startet den winzigen Window Manager im Hintergrund, damit Tastaturen in Apps funktionieren!
openbox &

# Soundkarte entmuten und Lautstaerke auf 100% setzen! (ALSA Mute-Bug fix)
amixer sset Master unmute || true
amixer sset Master 100% || true
amixer sset PCM unmute || true
amixer sset PCM 100% || true

# Start DBus session if not running, needed for pipewire
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    eval $(dbus-launch --sh-syntax)
fi

# XDG_RUNTIME_DIR muss gesetzt sein damit Firefox den PulseAudio-Socket findet!
export XDG_RUNTIME_DIR=/run/user/0
mkdir -p $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR

# Start Pipewire in background subshell without blocking the OS launch
( pipewire &
  sleep 1
  wireplumber &
  sleep 0.5
  pipewire-pulse &
) &

cd /opt/meinos
exec ./meinos.elf
EOF
chmod +x $ROOTFS/root/.xinitrc

cat > $ROOTFS/etc/systemd/system/meinos.service << 'EOF'
[Unit]
Description=Start MeinOS X11 Environment
After=systemd-user-sessions.service plymouth-quit-wait.service getty@tty1.service
Conflicts=getty@tty1.service

[Service]
ExecStart=/usr/bin/xinit /root/.xinitrc -- /usr/bin/X :0 -nolisten tcp vt1
Restart=always
User=root
StandardInput=tty
TTYPath=/dev/tty1

[Install]
WantedBy=graphical.target
EOF

chroot $ROOTFS /usr/bin/systemctl enable meinos.service || true
chroot $ROOTFS /usr/bin/systemctl set-default graphical.target || true

chroot $ROOTFS passwd -d root

umount $ROOTFS/dev/pts || true
umount $ROOTFS/sys || true
umount $ROOTFS/proc || true
umount $ROOTFS/dev || true

echo "6. Packaging the ISO..."
export ISO_DIR=/tmp/meinos_build/live_iso
rm -rf $ISO_DIR/live/filesystem.squashfs 
mkdir -p $ISO_DIR/live
mkdir -p $ISO_DIR/boot/grub

cp $ROOTFS/boot/vmlinuz-* $ISO_DIR/boot/vmlinuz
cp $ROOTFS/boot/initrd.img-* $ISO_DIR/boot/initrd.img

echo "Compressing filesystem (This will also take a while)..."

# HIER GREIFT JETZT DIE VARIABLE $COMP_ARGS AUS DEM MENÜ
# Wir lassen einen CPU-Kern frei (-processors 3), damit dein Desktop nicht mehr einfriert!
mksquashfs $ROOTFS $ISO_DIR/live/filesystem.squashfs $COMP_ARGS -noappend -processors 3 -e "proc/*" "sys/*" "dev/*" "tmp/*" "run/*"

cat > $ISO_DIR/boot/grub/grub.cfg << 'EOF'
set timeout=0
set timeout_style=hidden
set gfxpayload=text

menuentry "MeinOS Live" {
    set loopback=""
    if [ -n "$iso_path" ]; then
        set loopback="findiso=${iso_path} iso-scan/filename=${iso_path}"
    fi
    linux /boot/vmlinuz boot=live components rootwait quiet splash toram noprompt noeject $loopback
    initrd /boot/initrd.img
}
EOF

grub-mkrescue -o /tmp/CosmOS.iso $ISO_DIR
echo "-> Kopiere ISO auf SSD in sicheren, synchronen Blöcken (verhindert exFAT-Freeze!)..."
dd if=/tmp/CosmOS.iso of=CosmOS.iso bs=4M oflag=sync status=progress
rm -f /tmp/CosmOS.iso

echo "=========================================="
echo "          ISO ERFOLGREICH GEBAUT          "
echo "=========================================="