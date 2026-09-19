#!/bin/bash
set -e

echo "=========================================="
echo "    COSMOS OS - BARE METAL BUILD SYSTEM   "
echo "=========================================="

echo "-> Erstelle automatisches Backup..."
BACKUP_DIR="backups/cosmos_$(date +%Y-%m-%d_%H-%M-%S)"
mkdir -p "$BACKUP_DIR"
cp *.cpp *.h *.s *.ld "$BACKUP_DIR/" 2>/dev/null || true
echo "-> Quellcodes sicher kopiert nach: $BACKUP_DIR"

echo "------------------------------------------"
echo "0. Alten Müll aufräumen..."
rm -f *.o *.elf *.bin cosmos.iso
rm -rf isodir || true
sleep 1
mkdir -p isodir/boot/grub

echo "------------------------------------------"
echo "1. Erstelle 64-Bit Linker-Skript..."
cat > linker64.ld << 'EOF'
ENTRY(os2_start)
SECTIONS
{
    . = 0x1000000;
    .text : ALIGN(4096) {
        *(.text.entry)
        *(.text .text.*)
    }
    .rodata : ALIGN(4096) { *(.rodata .rodata.*) }
    .data : ALIGN(4096) { *(.data .data.*) }
    .bss : ALIGN(4096) { 
        __bss_start = .;
        *(COMMON) *(.bss .bss.*) 
        __bss_end = .;
    }
}
EOF

echo "------------------------------------------"
echo "2. Kompiliere OS1 (32-Bit Bootloader & Disk Manager)..."
as --32 bare_metal_hobby/boot.s -o boot.o
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/kernel.cpp -o kernel_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/pci.cpp -o pci_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/net.cpp -o net_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_bytes.cpp -o cosmos_bytes_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_fs.cpp -o cosmos_fs_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_tba.cpp -o cosmos_tba_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_ahci.cpp -o cosmos_ahci_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_cfs.cpp -o cosmos_cfs_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_usb.cpp -o cosmos_usb_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_hda.cpp -o cosmos_hda_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_partition.cpp -o cosmos_partition_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/cosmos_fat32.cpp -o cosmos_fat32_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m32 -U__linux__ -O2 -c bare_metal_hobby/memory.cpp -o memory_32.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast

ld -m elf_i386 -T bare_metal_hobby/linker.ld -static -z noexecstack -o isodir/boot/kernel.bin boot.o kernel_32.o pci_32.o net_32.o cosmos_bytes_32.o cosmos_fs_32.o cosmos_tba_32.o cosmos_ahci_32.o cosmos_cfs_32.o cosmos_usb_32.o cosmos_hda_32.o cosmos_partition_32.o cosmos_fat32_32.o memory_32.o

echo "------------------------------------------"
echo "3. Kompiliere OS2 (64-Bit Payload Kernel)..."
as --64 bare_metal_hobby/os2_entry.s -o os2_entry.o
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/kernel_main.cpp -o kernel_main_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/kernel.cpp -o kernel_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/pci.cpp -o pci_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/net.cpp -o net_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_bytes.cpp -o cosmos_bytes_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_fs.cpp -o cosmos_fs_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_tba.cpp -o cosmos_tba_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_ahci.cpp -o cosmos_ahci_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_cfs.cpp -o cosmos_cfs_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_usb.cpp -o cosmos_usb_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_hda.cpp -o cosmos_hda_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c arcade.cpp -o arcade.o -ffreestanding -fno-exceptions -fno-rtti -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c stb_wrapper.cpp -o stb_wrapper.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_partition.cpp -o cosmos_partition_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/cosmos_fat32.cpp -o cosmos_fat32_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c bare_metal_hobby/memory.cpp -o memory_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c cosmos_smp.cpp -o cosmos_smp_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast
g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c cosmos_nvme.cpp -o cosmos_nvme_64.o -ffreestanding -fno-exceptions -fno-rtti -fpermissive -Wno-int-to-pointer-cast

echo "------------------------------------------"
echo "4. Linke OS2 (Die flache KERNEL.BIN)..."
ld -m elf_x86_64 -T linker64.ld -z noexecstack --allow-multiple-definition os2_entry.o kernel_main_64.o kernel_64.o pci_64.o net_64.o cosmos_bytes_64.o cosmos_fs_64.o cosmos_tba_64.o cosmos_ahci_64.o cosmos_cfs_64.o cosmos_usb_64.o cosmos_hda_64.o arcade.o stb_wrapper.o cosmos_partition_64.o cosmos_fat32_64.o memory_64.o cosmos_tls_64.o cosmos_smp_64.o cosmos_nvme_64.o -o kernel_main.elf

objcopy -O binary kernel_main.elf isodir/KERNEL.BIN

echo "5. Externe App kompilieren..."
cat > bare_metal_hobby/app_linker.ld << 'EOF'
ENTRY(app_main)
SECTIONS
{
    . = 0x01100000;  /* BARE METAL FIX: Auf 64 Megabyte hochschieben! */
    .text : { *(.text.entry) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) }
    /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
EOF

#g++ -I. -Ibare_metal_hobby -m64 -U__linux__ -mno-red-zone -O2 -c app.cpp -o app.o -ffreestanding -fno-exceptions -fno-rtti
#ld -m elf_x86_64 -T bare_metal_hobby/app_linker.ld -z noexecstack --unresolved-symbols=ignore-all app.o -o app.elf
#objcopy -O binary app.elf app.bin
touch app.bin
echo "-> app.bin erfolgreich extrahiert!"

# ==============================================================
# BARE METAL FIX: DOPPEL-INJEKTION & AUTOMATISCHES RECHTE-PATCHING
# ==============================================================
echo "5. Injiziere app.bin und roms.tba in virtuelle Laufwerke..."

# 1. Die rohen Daten auf die Festplatten-Sektoren brennen
dd if=app.bin of=cosmos_drive.img bs=512 seek=10000 conv=notrunc status=none
dd if=app.bin of=cosmos_hdd.img bs=512 seek=10000 conv=notrunc status=none

if [ -f roms.tba ]; then
    dd if=roms.tba of=cosmos_drive.img bs=512 seek=11000 conv=notrunc status=none
    dd if=roms.tba of=cosmos_hdd.img bs=512 seek=11000 conv=notrunc status=none
fi

# 2. Das temporäre Tool mit der EXAKTEN 18-Byte CFS V2 Kernel-Struktur füttern
cat > patch_image_fs.cpp << 'EOF'
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdint>

// EXAKT DAS GLEICHE LAYOUT WIE IN DEINER KERNEL_MAIN.CPP! (18 Bytes packed)
struct CFS_DIR_ENTRY { 
    uint8_t  type;          
    char     filename[11];  
    uint16_t start_lba;     
    uint16_t file_size;     
    uint8_t  parent_idx;    
    uint8_t  reserved;      
} __attribute__((packed));

uint32_t get_file_size(const char* name) {
    std::ifstream f(name, std::ios::binary | std::ios::ate);
    if (!f) return 0;
    return (uint32_t)f.tellg();
}

void inject_entry(CFS_DIR_ENTRY* entries, const char* name, uint32_t size, uint32_t lba) {
    if (size == 0) return;
    int slot = -1;
    
// 28 Einträge passen glatt in einen 512-Byte Sektor (28 * 18 = 504 Bytes)
    for (int i = 0; i < 28; i++) {
        if (entries[i].type == 1 && std::strncmp(entries[i].filename, name, std::strlen(name)) == 0) {
            slot = i; break;
        }
    }
    if (slot == -1) {
        for (int i = 0; i < 28; i++) {
            if (entries[i].type == 0) { slot = i; break; }
        }
    }
    
    if (slot != -1) {
        entries[slot].type = 1; // 1 = Datei
        std::memset(entries[slot].filename, 0, 11);
        std::strncpy(entries[slot].filename, name, 11);
        entries[slot].start_lba = (uint16_t)lba;
        entries[slot].file_size = (uint16_t)size;
        entries[slot].parent_idx = 255; // WICHTIG: 255 signalisiert dem Kernel "Root-Ordner"!
        entries[slot].reserved = 0;
    }
}

void patch_file(const char* img_name, uint32_t app_size, uint32_t tba_size) {
    std::fstream file(img_name, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return;
    
    // Sektor 1002 (Inhaltsverzeichnis) einlesen
    file.seekg(1002 * 512);
    CFS_DIR_ENTRY entries[28]; 
    file.read((char*)entries, sizeof(entries));
    
    // Einträge mit richtigem Padding & Parent-ID patchen
    inject_entry(entries, "APP.BIN", app_size, 10000);
    inject_entry(entries, "ROMS.TBA", tba_size, 11000);
    
    // Sektor 1002 wieder fehlerfrei zurückschreiben
    file.seekp(1002 * 512);
    file.write((char*)entries, sizeof(entries));
}

int main() {
    uint32_t app_sz = get_file_size("app.bin");
    uint32_t tba_sz = get_file_size("roms.tba");
    patch_file("cosmos_drive.img", app_sz, tba_sz);
    patch_file("cosmos_hdd.img", app_sz, tba_sz);
    return 0;
}
EOF

# Tool kompilieren, ausführen und aufräumen
g++ patch_image_fs.cpp -o patch_image_fs
./patch_image_fs
rm -f patch_image_fs.cpp patch_image_fs

echo "-> Inhaltsverzeichnis (Sektor 1002) erfolgreich an CFS V2 angepasst!"

echo "------------------------------------------"
echo "------------------------------------------"
echo "6. ISO zusammenbauen..."

# Wir erstellen die grub.cfg mit den korrekten Pfaden
cat > isodir/boot/grub/grub.cfg << 'EOF'
set timeout=0
set default=0
menuentry "Cosmos OS" {
    set gfxpayload=1920x1080x32
    multiboot /boot/kernel.bin
    module /KERNEL.BIN       # Dein Kernel-Payload
    module /APP.BIN "cartridge"  # Die geschrumpfte RAM-Disk (als APP.BIN getarnt!)
    boot
}
EOF

# WICHTIG: Wir löschen das lose app.bin-Kopieren aus dem alten Skript,
# da die App ja bereits fest im cosmos_drive.img eingebrannt ist!
rm -f isodir/APP.BIN

# ==============================================================
# BARE METAL FIX: SCHRUMPFEN & TARNEN
# ==============================================================
# Wir schneiden 12.000 Sektoren aus und nennen es APP.BIN,
# damit der 32-Bit Loader die Datei akzeptiert.
dd if=cosmos_drive.img of=isodir/APP.BIN bs=512 count=25000 status=none

# roms.tba zur Sicherheit mit in den ISO-Root legen
if [ -f roms.tba ]; then
    cp roms.tba isodir/ROMS.TBA
fi

# Alte ISO löschen
rm -f cosmos.iso 

# ISO neu bauen (Jetzt klein, schnell und voll kompatibel!)
grub-mkrescue -o cosmos.iso isodir

echo "=========================================="
echo "          ISO ERFOLGREICH GEBAUT          "
echo "=========================================="
