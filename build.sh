#!/bin/bash
set -e

GCC=x86_64-elf-gcc
LD=x86_64-elf-ld

FLAGS="-m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx"

echo "=== Kernel entry ==="
$GCC $FLAGS -c kernel_entry.S -o kernel_entry.o

echo "=== Heap ==="
$GCC $FLAGS -c kmalloc.c -o kmalloc.o

echo "=== Utils ==="
$GCC $FLAGS -c utils.c -o utils.o

echo "=== Framebuffer ==="
$GCC $FLAGS -c framebuffer.c -o framebuffer.o

echo "=== Kernel ==="
$GCC $FLAGS -c kernel.c -o kernel.o

echo "=== Link ==="
$LD -m elf_i386 -T linker.ld -o colibri.cos \
    --start-group \
    kernel_entry.o kernel.o kmalloc.o utils.o framebuffer.o \
    --end-group

ls -l colibri.cos

echo "=== Run ==="
qemu-system-i386 -kernel colibri.cos -m 16M -net none -vga std