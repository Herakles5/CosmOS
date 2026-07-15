#!/bin/bash
# install_cosmos.sh

if [ "$EUID" -ne 0 ]; then
  echo "ERROR: The installer must be run as root!"
  sleep 3
  exit 1
fi

clear
echo "=========================================="
echo "    CosmOS / MeinOS System Installer      "
echo "=========================================="
echo "How would you like to install?"
echo ""
echo "[1] ERASE ENTIRE DISK (CosmOS as the only system)"
echo "    -> Automatically creates an EFI partition and formats the drive."
echo ""
echo "[2] MANUAL / MULTIBOOT (Install alongside Windows)"
echo "    -> You select the partitions yourself."
echo ""
echo "[3] UPDATE (Update existing CosmOS)"
echo "    -> System files will be overwritten. Your data will be preserved!"
echo "=========================================="
read -p "Select [1], [2] or [3]: " INSTALL_MODE

if [ "$INSTALL_MODE" == "1" ]; then
    echo ""
    echo "Available drives (WARNING: Drive will be completely erased!):"
    lsblk -o NAME,SIZE,TYPE | grep "disk" | grep -v "loop"
    echo ""
    read -p "Which drive should be erased? (e.g. /dev/sda or /dev/nvme0n1): " TARGET_DISK
    
    if [ ! -b "$TARGET_DISK" ]; then
        echo "Drive not found!"
        sleep 3
        exit 1
    fi
    
    read -p "Are you SURE you want to COMPLETELY ERASE $TARGET_DISK? (y/N): " CONFIRM
    if [[ "$CONFIRM" != "y" && "$CONFIRM" != "Y" ]]; then exit 1; fi
    
    echo "-> Partitioning $TARGET_DISK..."
    if [ -d /sys/firmware/efi ]; then
        # UEFI
        parted -s "$TARGET_DISK" mklabel gpt
        parted -s "$TARGET_DISK" mkpart ESP fat32 1MiB 300MiB
        parted -s "$TARGET_DISK" set 1 esp on
        parted -s "$TARGET_DISK" mkpart primary ext4 300MiB 100%
        partprobe "$TARGET_DISK"
        sleep 3
        if [[ "$TARGET_DISK" =~ [0-9]$ ]]; then
            EFI_PART="${TARGET_DISK}p1"
            TARGET_PART="${TARGET_DISK}p2"
        else
            EFI_PART="${TARGET_DISK}1"
            TARGET_PART="${TARGET_DISK}2"
        fi
        mkfs.fat -F32 "$EFI_PART"
        mkfs.ext4 -F "$TARGET_PART"
    else
        # Legacy BIOS / MBR (Fixes Freeze for Intel Matrix Storage Manager!)
        parted -s "$TARGET_DISK" mklabel msdos
        parted -s "$TARGET_DISK" mkpart primary ext4 1MiB 100%
        parted -s "$TARGET_DISK" set 1 boot on
        partprobe "$TARGET_DISK"
        sleep 3
        if [[ "$TARGET_DISK" =~ [0-9]$ ]]; then
            TARGET_PART="${TARGET_DISK}p1"
        else
            TARGET_PART="${TARGET_DISK}1"
        fi
        EFI_PART=""
        mkfs.ext4 -F "$TARGET_PART"
    fi

elif [ "$INSTALL_MODE" == "2" ]; then
    echo ""
    echo "Available partitions:"
    lsblk -o NAME,SIZE,FSTYPE,MOUNTPOINT -p | grep -v "loop" | grep -v "squashfs"
    echo ""
    read -p "Enter target partition for CosmOS (e.g. /dev/sda3): " TARGET_PART
    
    if [ ! -b "$TARGET_PART" ]; then exit 1; fi
    read -p "Format $TARGET_PART? (y/N): " CONFIRM
    if [[ "$CONFIRM" != "y" && "$CONFIRM" != "Y" ]]; then exit 1; fi
    
    mkfs.ext4 -F "$TARGET_PART"
    
    echo ""
    echo "HINT: Select the EFI partition that is approx. 100MB in size (often /dev/nvme0n1p1 or /dev/sda1)."
    lsblk -o NAME,SIZE,FSTYPE | grep -i "vfat"
    read -p "Please enter EFI partition: " EFI_PART
    
    TARGET_DISK=$(echo "$TARGET_PART" | sed 's/[0-9]*$//')
    if [[ "$TARGET_DISK" =~ p$ ]]; then
        TARGET_DISK=$(echo "$TARGET_DISK" | sed 's/p$//')
    fi
elif [ "$INSTALL_MODE" == "3" ]; then
    echo ""
    echo "Available partitions:"
    lsblk -o NAME,SIZE,FSTYPE,MOUNTPOINT -p | grep -v "loop" | grep -v "squashfs"
    echo ""
    read -p "Enter target partition for CosmOS Update (e.g. /dev/sda3): " TARGET_PART
    
    if [ ! -b "$TARGET_PART" ]; then exit 1; fi
    
    echo "Checking partition for CosmOS installation..."
    mkdir -p /mnt/target_check
    mount "$TARGET_PART" /mnt/target_check || { echo "ERROR: Could not mount partition!"; sleep 5; exit 1; }
    if [ ! -d "/mnt/target_check/boot" ]; then
        echo "ERROR: No valid system found on this partition!"
        umount /mnt/target_check
        sleep 5
        exit 1
    fi
    umount /mnt/target_check
    
    echo "The system will now be updated. Data will be preserved."
    
    echo ""
    lsblk -o NAME,SIZE,FSTYPE | grep -i "vfat"
    read -p "Please enter EFI partition (if available, otherwise leave empty): " EFI_PART
    
    TARGET_DISK=$(echo "$TARGET_PART" | sed 's/[0-9]*$//')
    if [[ "$TARGET_DISK" =~ p$ ]]; then
        TARGET_DISK=$(echo "$TARGET_DISK" | sed 's/p$//')
    fi
else
    echo "Aborted."
    exit 1
fi

echo ""
echo "-> Copying CosmOS System..."
mkdir -p /mnt/target
mount "$TARGET_PART" /mnt/target || { echo "ERROR: Could not mount partition $TARGET_PART!"; sleep 5; exit 1; }
rsync -aAX --exclude={"/dev/*","/proc/*","/sys/*","/tmp/*","/run/*","/mnt/*","/media/*","/cdrom/*","/rofs/*","/lib/live/mount/*"} / /mnt/target/

echo "-> Configuring Bootloader..."
mkdir -p /mnt/target/boot/efi
if [ -n "$EFI_PART" ]; then
    mount "$EFI_PART" /mnt/target/boot/efi
fi

mount --bind /dev /mnt/target/dev
mount --bind /proc /mnt/target/proc
mount --bind /sys /mnt/target/sys

if [ -d /sys/firmware/efi/efivars ]; then
    mount --bind /sys/firmware/efi/efivars /mnt/target/sys/firmware/efi/efivars
fi

echo "GRUB_DISABLE_OS_PROBER=false" >> /mnt/target/etc/default/grub
sed -i 's/GRUB_TIMEOUT_STYLE=hidden/GRUB_TIMEOUT_STYLE=menu/g' /mnt/target/etc/default/grub
sed -i 's/GRUB_TIMEOUT=0/GRUB_TIMEOUT=2/g' /mnt/target/etc/default/grub
sed -i 's/GRUB_TIMEOUT=10/GRUB_TIMEOUT=2/g' /mnt/target/etc/default/grub

if [ -d /sys/firmware/efi ]; then
    # 1. Normal UEFI entry (Registers in Motherboard NVRAM)
    chroot /mnt/target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=CosmOS --recheck
    
    # 2. Fallback Bootloader! IMPORTANT FOR EMPTY PCs! (Creates EFI/BOOT/BOOTX64.EFI)
    chroot /mnt/target grub-install --target=x86_64-efi --efi-directory=/boot/efi --removable
else
    chroot /mnt/target grub-install "$TARGET_DISK"
fi

echo "-> Removing Live CD components from target system (prevents shutdown freeze)..."
chroot /mnt/target dpkg --purge live-boot live-tools live-boot-initramfs-tools 2>/dev/null || true
chroot /mnt/target apt-get remove --purge -y live-boot live-tools live-boot-initramfs-tools 2>/dev/null || true
chroot /mnt/target update-initramfs -u

chroot /mnt/target update-grub

# Hijack Trick only apply if Windows exists and Mode 2 was chosen
if [ "$INSTALL_MODE" == "2" ] && [ -d /sys/firmware/efi ]; then
    MS_BOOT="/mnt/target/boot/efi/EFI/Microsoft/Boot"
    if [ -f "$MS_BOOT/bootmgfw.efi" ]; then
        echo "-> Applying Windows Boot Hijack..."
        if [ ! -f "$MS_BOOT/bootmgfw-orig.efi" ]; then
            mv "$MS_BOOT/bootmgfw.efi" "$MS_BOOT/bootmgfw-orig.efi"
        fi
        if [ -f "/mnt/target/boot/efi/EFI/CosmOS/grubx64.efi" ]; then
            cp "/mnt/target/boot/efi/EFI/CosmOS/grubx64.efi" "$MS_BOOT/bootmgfw.efi"
        fi
        sed -i 's/bootmgfw.efi/bootmgfw-orig.efi/g' /mnt/target/boot/grub/grub.cfg
    fi
fi

# Cleanup
if [ -d /sys/firmware/efi/efivars ]; then
    umount /mnt/target/sys/firmware/efi/efivars || true
fi
umount /mnt/target/sys
umount /mnt/target/proc
umount /mnt/target/dev
umount /mnt/target/boot/efi 2>/dev/null || true
umount /mnt/target

echo ""
echo "=========================================="
echo "         INSTALLATION SUCCESSFUL!         "
echo "=========================================="
read -p "Press ENTER to close the installer..."