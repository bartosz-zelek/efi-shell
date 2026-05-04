#!/bin/bash

gcc -Ignu-efi/inc -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -c main.c -o main.o

ld -shared -Bsymbolic -Lgnu-efi/x86_64/lib -Lgnu-efi/x86_64/gnuefi -Tgnu-efi/gnuefi/elf_x86_64_efi.lds gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o main.o -o main.so -lgnuefi -lefi

objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --output-target efi-app-x86_64 --subsystem=10 main.so main.efi

# Setup QEMU and UEFI environment
mkdir -p esp/EFI/BOOT
cp main.efi esp/EFI/BOOT/BOOTX64.EFI
echo "\EFI\BOOT\BOOTX64.EFI" > esp/startup.nsh

# Find OVMF firmware
OVMF_PATH="/usr/share/ovmf/OVMF.fd"
if [ ! -f "$OVMF_PATH" ]; then
    OVMF_PATH="/usr/share/OVMF/OVMF.fd"
fi

if [ ! -f "$OVMF_PATH" ]; then
    echo "Error: OVMF firmware not found. Install it with: sudo apt install ovmf"
    exit 1
fi

# Run QEMU with UEFI firmware
echo "Starting QEMU with UEFI firmware..."
qemu-system-x86_64 -m 256 -vga std -bios "$OVMF_PATH" -drive file=fat:rw:esp,format=raw -net none