#!/bin/bash
CHROOT_DIR="/media/root/fc87ca2c-9ad8-4363-b812-a426bd4d1534"
echo "Mounting file systems..."
mount --bind /dev $CHROOT_DIR/dev
mount --bind /proc $CHROOT_DIR/proc
mount --bind /sys $CHROOT_DIR/sys
mount --bind /sys/firmware/efi/efivars $CHROOT_DIR/sys/firmware/efi/efivars
mount /dev/sda1 $CHROOT_DIR/boot/efi

echo "Reinstalling GRUB..."
chroot $CHROOT_DIR grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=ubuntu --recheck
echo "Updating GRUB config..."
chroot $CHROOT_DIR update-grub

echo "Unmounting file systems..."
umount $CHROOT_DIR/boot/efi
umount $CHROOT_DIR/sys/firmware/efi/efivars
umount $CHROOT_DIR/sys
umount $CHROOT_DIR/proc
umount $CHROOT_DIR/dev
echo "Done!"
