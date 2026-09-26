#!/bin/bash
set -e

GCC=x86_64-elf-gcc
LD=x86_64-elf-ld

echo "=== Kernel entry ==="
$GCC -m32 -ffreestanding -fno-pie -c kernel_entry.S -o kernel_entry.o

echo "=== Heap ==="
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c kmalloc.c -o kmalloc.o

echo "=== Utils ==="
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c utils.c -o utils.o

echo "=== Kernel ==="
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c kernel.c -o kernel.o
# Link
echo "=== Link ==="
$LD -m elf_i386 -T linker.ld -o colibri.cos \
    --start-group \
    kernel_entry.o kernel.o kmalloc.o utils.o \
    --end-group

echo "colibri.cos:"
ls -l colibri.cos
# Run
echo "=== Run ==="
qemu-system-i386 -kernel colibri.cos -m 16M -net none -vga std