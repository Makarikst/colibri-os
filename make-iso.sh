#!/bin/bash
set -e

GCC=x86_64-elf-gcc
LD=x86_64-elf-ld

echo "=== Build kernel ==="
$GCC -m32 -ffreestanding -fno-pie -c kernel_entry.S -o kernel_entry.o
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c kmalloc.c -o kmalloc.o
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c utils.c -o utils.o
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c framebuffer.c -o framebuffer.o
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c kernel.c -o kernel.o

echo "=== Link ==="
$LD -m elf_i386 -T linker.ld -o colibri.cos \
    --start-group \
    kernel_entry.o kernel.o kmalloc.o utils.o framebuffer.o \
    --end-group

ls -l colibri.cos

echo "=== Prepare ISO ==="
rm -rf iso/                        # ← чистим только iso/
mkdir -p iso/boot/grub
cp colibri.cos iso/boot/colibri.cos
cp boot/grub/grub.cfg iso/boot/grub/grub.cfg    # ← из boot/grub/

echo "=== Create ISO ==="
i686-elf-grub-mkrescue -o colibri.iso iso/

ls -l colibri.iso

echo "=== Run GRUB ==="
qemu-system-i386 -cdrom colibri.iso -m 16M -net none -vga std