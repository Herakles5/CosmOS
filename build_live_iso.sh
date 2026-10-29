#!/bin/bash
set -e


echo "=========================================="
echo "    CosmOS Live Build System              "
echo "=========================================="
echo "Which ISO version would you like to build?"
echo " [1] FAST MODE    (Zstd: Builds in seconds, ~1.2 GB, ideal for quick testing)"
echo " [2] RELEASE MODE (XZ Max: Maximum compression, smallest footprint for hardware)"
read -t 5 -p "Select 1 or 2 (Default is 1 after 5 seconds): " BUILD_MODE || BUILD_MODE="1"
if [ -z "$BUILD_MODE" ]; then BUILD_MODE="1"; fi

if [ "$BUILD_MODE" == "2" ]; then
    # Ultra-high compression with 1MB block size. Memory limited to 2GB to prevent crashes!
    COMP_ARGS="-comp xz -b 1M -Xbcj x86 -mem 2G"
    echo "-> RELEASE MODE activated. Grabbing a coffee, this will take some time..."
    sleep 2
else
    # High-speed compression
    COMP_ARGS="-comp zstd -Xcompression-level 3"
    echo "-> FAST MODE activated. Turbo speed engaged!"
    sleep 1
fi

echo "------------------------------------------"
echo "-> Creating automated backup..."
BACKUP_DIR="backups/cosmos_$(date +%Y-%m-%d_%H-%M-%S)"
mkdir -p "$BACKUP_DIR"
cp *.cpp *.h *.s *.ld "$BACKUP_DIR/" 2>/dev/null || true
echo "-> Source codes safely backed up to: $BACKUP_DIR"

echo "------------------------------------------"
echo "0. Cleaning up old build artifacts..."

echo "1. Cleaning up old build files..."
rm -f CosmOS.iso
rm -rf /media/root/500GB/build_tmp_live_iso 2>/dev/null || true

# Auto-mount 500GB storage if configured
mount /media/root/500GB 2>/dev/null || true

# FIX: Remount /tmp with dev+exec permissions so debootstrap can create device nodes!
mount -o remount,dev,exec /tmp 2>/dev/null || true

BUILD_DIR="$PWD/build_tmp"
mkdir -p "$BUILD_DIR"

# Clean up previous build files - ENSURE NOTHING REMAINS MOUNTED!
umount "$BUILD_DIR/live_rootfs/dev/pts" 2>/dev/null || true
umount "$BUILD_DIR/live_rootfs/dev" 2>/dev/null || true
umount "$BUILD_DIR/live_rootfs/sys" 2>/dev/null || true
umount "$BUILD_DIR/live_rootfs/proc" 2>/dev/null || true

rm -rf "$BUILD_DIR"/live_rootfs "$BUILD_DIR"/live_iso
mkdir -p "$BUILD_DIR"/live_rootfs "$BUILD_DIR"/live_iso
echo "2. Compiling meinos.elf (Linux version)..."
make -f Makefile.linux clean
make -f Makefile.linux

echo "3. Creating minimal root filesystem (This will take a while)..."
export ROOTFS="$BUILD_DIR/live_rootfs"
mkdir -p $ROOTFS
if [ ! -f "$ROOTFS/bin/bash" ]; then
    debootstrap --include=gpgv,ubuntu-keyring,gnupg,apt-utils --arch=amd64 noble $ROOTFS http://archive.ubuntu.com/ubuntu/
else
    echo "Base system already extracted, skipping debootstrap."
fi

echo "4. Configuring the root filesystem..."
mount -t proc none $ROOTFS/proc || true
mount -t sysfs none $ROOTFS/sys || true
mount -o bind /dev $ROOTFS/dev || true
mount -o bind /dev/pts $ROOTFS/dev/pts || true

# Ensure network works inside chroot
rm -f $ROOTFS/etc/resolv.conf
echo "nameserver 8.8.8.8" > $ROOTFS/etc/resolv.conf

# Install necessary packages inside chroot
chroot $ROOTFS /bin/bash << 'EOF'
set -e
export DEBIAN_FRONTEND=noninteractive

# Populate complete package sources (prevents "Unable to locate package")
rm -f /etc/apt/sources.list.d/ubuntu.sources
cat << 'SOURCES' > /etc/apt/sources.list
deb [trusted=yes] http://archive.ubuntu.com/ubuntu/ noble main restricted universe multiverse
deb [trusted=yes] http://archive.ubuntu.com/ubuntu/ noble-updates main restricted universe multiverse
deb [trusted=yes] http://security.ubuntu.com/ubuntu noble-security main restricted universe multiverse
SOURCES

# TURBO-BOOST: Disable fsync in dpkg. Prevents heavy I/O lags on USB/exFAT storage!
echo "force-unsafe-io" > /etc/dpkg/dpkg.cfg.d/force-unsafe-io

# FIX FOR exFAT/USB DRIVES: Prevent apt from dropping root privileges.
# Unprivileged _apt user cannot access gpgv or /dev/null due to exFAT folder permission masks!
echo 'APT::Sandbox::User "root";' > /etc/apt/apt.conf.d/99sandbox

apt-get update

# Install and configure standard locales (fixes Wine/EA App Arabic language bug)
apt-get install -y locales
echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen
echo "de_DE.UTF-8 UTF-8" >> /etc/locale.gen
locale-gen
update-locale LANG=en_US.UTF-8

# === COSMOS BASE & INSTALLER PACKAGES ===
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
    ffmpeg wireplumber dbus-x11 pavucontrol \
    debootstrap squashfs-tools xorriso mtools \
    udisks2 gvfs gvfs-backends polkitd

# === FIREFOX (WITHOUT SNAP VIA MOZILLA PPA) ===
rm -f /etc/resolv.conf
echo "nameserver 8.8.8.8" > /etc/resolv.conf
install -d -m 0755 /etc/apt/keyrings
wget https://packages.mozilla.org/apt/repo-signing-key.gpg -O- > /etc/apt/keyrings/packages.mozilla.org.asc
echo 'deb [signed-by=/etc/apt/keyrings/packages.mozilla.org.asc] https://packages.mozilla.org/apt mozilla main' > /etc/apt/sources.list.d/mozilla.list
echo 'Package: *
Pin: origin packages.mozilla.org
Pin-Priority: 1000' > /etc/apt/preferences.d/mozilla
apt-get update
apt-get install -y firefox

# === ZEROTIER NETWORK (P2P MESH VPN) ===
echo "-> Installing ZeroTier (CosmOS Network)..."
apt-get install -y curl gpg
curl -s https://install.zerotier.com | bash

# === STEAM (Gaming Platform) ===
echo "-> Installing Steam..."
dpkg --add-architecture i386
apt-get update
apt-get install -y --no-install-recommends steam-installer || echo "WARN: Steam install failed, continuing..."

# === WINE (Windows Compatibility Layer for EA/Windows Games) ===
echo "-> Installing Wine..."
apt-get install -y --no-install-recommends \
    wine64 wine32 winetricks || echo "WARN: Wine install failed, continuing..."

# === LUTRIS (Game Manager for EA App + GOG + Epic) ===
echo "-> Installing Lutris..."
apt-get install -y --no-install-recommends \
    lutris pciutils || echo "WARN: Lutris install failed, continuing..."

# Fix Lutris root check and create symlinks for Openbox compatibility
sed -i 's/if os.geteuid() == 0:/if False:/g' /usr/lib/python3/dist-packages/lutris/gui/application.py || true
ln -sf /usr/games/lutris /usr/bin/lutris || true
ln -sf /usr/games/steam /usr/bin/steam || true

# === GPU DRIVERS (Vulkan support for Proton/Wine) ===
echo "-> Installing GPU/Vulkan Drivers..."
apt-get install -y --no-install-recommends \
    mesa-vulkan-drivers vulkan-tools libvulkan1 \
    mesa-utils || echo "WARN: Vulkan install failed, continuing..."

apt-get clean
# Clean unneeded package cache to save space in final ISO
rm -rf /var/lib/apt/lists/*
rm -rf /tmp/*
EOF

# === NETWORK CONFIGURATION (LAN + WiFi + Bluetooth) ===
echo "4.5 Configuring Networking (LAN + WiFi + BT)..."

# Only loopback in interfaces, everything else is managed by NetworkManager
mkdir -p $ROOTFS/etc/network
echo "auto lo" > $ROOTFS/etc/network/interfaces
echo "iface lo inet loopback" >> $ROOTFS/etc/network/interfaces

# Enable NetworkManager for all devices (LAN, WiFi, BT)
# Disable other networking daemons to prevent conflicts
chroot $ROOTFS /usr/bin/systemctl enable NetworkManager || true
chroot $ROOTFS /usr/bin/systemctl disable systemd-networkd || true
chroot $ROOTFS /usr/bin/systemctl disable systemd-resolved || true
chroot $ROOTFS /usr/bin/systemctl mask systemd-networkd || true
chroot $ROOTFS /usr/bin/systemctl mask systemd-resolved || true
chroot $ROOTFS /usr/bin/systemctl mask systemd-networkd-wait-online.service || true

# Remove leftover systemd-networkd configurations
rm -rf $ROOTFS/etc/systemd/network/*

# NetworkManager: Manage all interfaces (not just WiFi)
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

# DHCP: Use permanent MAC address for router compatibility
mkdir -p $ROOTFS/etc/NetworkManager/conf.d
cat > $ROOTFS/etc/NetworkManager/conf.d/20-dhcp-mac.conf << 'DHCPEOF'
[main]
dhcp=internal

[ipv4]
dhcp-client-id=mac
method=auto
DHCPEOF

# DNS: Fallback DNS resolvers
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
# Create APP.BIN trigger file
echo "MEINOS_APP_TRIGGER" > $ROOTFS/opt/meinos/APP.BIN
cp ROMS.tba $ROOTFS/opt/meinos/ || true
cp -r sounds $ROOTFS/opt/meinos/ || true
cp install_cosmos.sh $ROOTFS/opt/meinos/ || true
chmod +x $ROOTFS/opt/meinos/install_cosmos.sh || true
cp face.png $ROOTFS/opt/meinos/ || true
cp -r "Antigravity IDE" $ROOTFS/opt/meinos/ || true
echo "-> Copying Surreal 3D Engine & Live Dojo..."
cp -r surreal_engine $ROOTFS/opt/meinos/ || true
cp run_surreal_dojo.sh $ROOTFS/opt/meinos/ || true
chmod +x $ROOTFS/opt/meinos/run_surreal_dojo.sh || true

echo "-> Copying Dual Audio Config..."
mkdir -p $ROOTFS/etc/pipewire/pipewire-pulse.conf.d
cp /etc/pipewire/pipewire-pulse.conf.d/50-dual-out.conf $ROOTFS/etc/pipewire/pipewire-pulse.conf.d/ || true

echo "-> Copying AI_Desktop (Whisper, Piper, Models) to ISO..."
mkdir -p $ROOTFS/root/coding/AI_Desktop
cp -r /root/coding/AI_Desktop/* $ROOTFS/root/coding/AI_Desktop/ || true

echo "-> Copying Ollama AI Backend to ISO..."
mkdir -p $ROOTFS/usr/local/bin
cp /usr/local/bin/ollama $ROOTFS/usr/local/bin/ollama || true
chmod +x $ROOTFS/usr/local/bin/ollama || true
mkdir -p $ROOTFS/usr/share/ollama
cp -r /usr/share/ollama/* $ROOTFS/usr/share/ollama/ || true

echo "-> Copying VM ISOs, OpenCore EFI, and XMLs to Live ISO..."
mkdir -p $ROOTFS/var/lib/libvirt/images
# Copy VM assets: include OpenCore.qcow2 bootloader, exclude large disk images (macOS.qcow2, Win10_Gaming.qcow2)
rsync -av --include="*/" --include="OpenCore.qcow2" --exclude="*.qcow2" /var/lib/libvirt/images/ $ROOTFS/var/lib/libvirt/images/ || true

cat > $ROOTFS/root/.xinitrc << 'EOF'
#!/bin/sh
xset -dpms
xset s off
xset s noblank

# Openbox Keybinding: PrintScreen = Screenshot
mkdir -p $ROOTFS/root/.config/openbox
cp $ROOTFS/etc/xdg/openbox/rc.xml $ROOTFS/root/.config/openbox/rc.xml
sed -i 's/<command>scrot -s<\/command>/<command>xfce4-screenshooter -w<\/command>/g' $ROOTFS/root/.config/openbox/rc.xml
sed -i 's/<command>scrot<\/command>/<command>xfce4-screenshooter -f<\/command>/g' $ROOTFS/root/.config/openbox/rc.xml

# Start lightweight window manager in background for application keyboard focus
openbox &

# Unmute soundcard and set master volume to 100%
amixer sset Master unmute || true
amixer sset Master 100% || true
amixer sset PCM unmute || true
amixer sset PCM 100% || true

# Start DBus session if not running, needed for pipewire
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    eval $(dbus-launch --sh-syntax)
fi

# Set XDG_RUNTIME_DIR so audio and desktop sockets are reachable
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

# Start Ollama AI Backend
( ollama serve ) &

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

# Auto-login directly as root on all TTYs (passwordless)
mkdir -p $ROOTFS/etc/systemd/system/getty@.service.d $ROOTFS/etc/systemd/system/getty@tty1.service.d
cat << 'GETTYEOF' > $ROOTFS/etc/systemd/system/getty@.service.d/override.conf
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin root --noclear %I $TERM
GETTYEOF
cp $ROOTFS/etc/systemd/system/getty@.service.d/override.conf $ROOTFS/etc/systemd/system/getty@tty1.service.d/override.conf


umount $ROOTFS/dev/pts || true
umount $ROOTFS/sys || true
umount $ROOTFS/proc || true
umount $ROOTFS/dev || true

echo "6. Packaging the ISO..."
# Auto-detect 500GB high-capacity storage for ISO staging and final image
if mountpoint -q /media/root/500GB || mount /media/root/500GB 2>/dev/null; then
    echo "-> 500GB storage detected. Staging squashfs & final ISO on /media/root/500GB to avoid filling root partition..."
    export ISO_DIR="/media/root/500GB/build_tmp_live_iso"
    OUTPUT_ISO_DIR="/media/root/500GB/ISOs"
    mkdir -p "$OUTPUT_ISO_DIR"
    FINAL_ISO="$OUTPUT_ISO_DIR/CosmOS.iso"
else
    echo "-> Staging ISO locally in $BUILD_DIR/live_iso..."
    export ISO_DIR="$BUILD_DIR/live_iso"
    FINAL_ISO="$PWD/CosmOS.iso"
fi

rm -rf "$ISO_DIR" 
mkdir -p "$ISO_DIR/live"
mkdir -p "$ISO_DIR/boot/grub"

cp $ROOTFS/boot/vmlinuz-* $ISO_DIR/boot/vmlinuz
cp $ROOTFS/boot/initrd.img-* $ISO_DIR/boot/initrd.img

echo "Compressing filesystem (This will also take a while)..."

# Apply compression settings selected in menu
# Leave one CPU core free (-processors 3) to keep desktop responsive
mksquashfs $ROOTFS $ISO_DIR/live/filesystem.squashfs $COMP_ARGS -noappend -processors 3 -e "proc/*" "sys/*" "dev/*" "tmp/*" "run/*"

cat > $ISO_DIR/boot/grub/grub.cfg << 'EOF'
set timeout=5
set timeout_style=menu
set gfxpayload=text

menuentry "MeinOS Live" {
    set loopback=""
    if [ -n "$iso_path" ]; then
        set loopback="findiso=${iso_path} iso-scan/filename=${iso_path}"
    fi
    linux /boot/vmlinuz boot=live components rootwait rootdelay=10 quiet splash noprompt noeject $loopback
    initrd /boot/initrd.img
}
EOF

echo "-> Creating bootable hybrid ISO directly to: $FINAL_ISO ..."
grub-mkrescue -o "$FINAL_ISO" "$ISO_DIR" -iso-level 3

if [ "$FINAL_ISO" != "$PWD/CosmOS.iso" ]; then
    ln -sf "$FINAL_ISO" "$PWD/CosmOS.iso"
    echo "-> Created symlink: $PWD/CosmOS.iso -> $FINAL_ISO"
fi

# Reclaim temporary squashfs build folder to save disk space
if [ "$ISO_DIR" = "/media/root/500GB/build_tmp_live_iso" ]; then
    echo "-> Reclaiming temporary staging files..."
    rm -rf "$ISO_DIR"
fi

echo "=========================================="
echo "          ISO BUILT SUCCESSFULLY          "
echo "  Location: $FINAL_ISO                    "
echo "=========================================="