#!/bin/bash
set -e
echo "Starting VM setup for Mac and Android..."
DIR="/var/lib/libvirt/images"
cd $DIR

# 1. Android
echo "[1/4] Downloading Android-x86 9.0..."
if [ ! -f "android-x86.iso" ]; then
    wget -qO android-x86.iso "https://osdn.net/dl/android-x86/android-x86_64-9.0-r2.iso" || \
    wget -qO android-x86.iso "https://nav.dl.sourceforge.net/project/android-x86/Release%209.0/android-x86_64-9.0-r2.iso" || \
    wget -qO android-x86.iso "https://jaist.dl.sourceforge.net/project/android-x86/Release%209.0/android-x86_64-9.0-r2.iso"
fi

if [ ! -f "Android.qcow2" ]; then
    qemu-img create -f qcow2 Android.qcow2 16G
fi
virsh define Android.xml || true

# 2. macOS (Monterey)
echo "[2/4] Setting up macOS (OSX-KVM)..."
cd OSX-KVM
echo "[3/4] Fetching macOS Monterey BaseSystem..."
if [ ! -f "BaseSystem.img" ]; then
    ./fetch-macOS-v2.py -s Monterey
    qemu-img convert BaseSystem.dmg -O raw BaseSystem.img
fi

echo "[4/4] Creating macOS VM disk and XML..."
if [ ! -f "../macOS.qcow2" ]; then
    qemu-img create -f qcow2 ../macOS.qcow2 64G
fi

# Define macOS VM from template
sed -e "s|/home/user/OSX-KVM|$DIR/OSX-KVM|g" macOS-libvirt-Catalina.xml > ../macOS.xml
sed -i "s|macOS.qcow2|$DIR/macOS.qcow2|g" ../macOS.xml
sed -i "s|<name>macOS-Simple-KVM</name>|<name>macOS</name>|g" ../macOS.xml
virsh define ../macOS.xml || true

echo "Setup complete! Both VMs are ready to boot."
