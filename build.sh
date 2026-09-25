#!/bin/bash
# ============================================
# Colibri OS — сборка v0.8 (VESA)
# ============================================

GCC=x86_64-elf-gcc
LD=x86_64-elf-ld
OBJCOPY=x86_64-elf-objcopy

echo "🔨 Сборка загрузчика..."
$GCC -m16 -ffreestanding -c boot.S -o boot.o
$LD -m elf_i386 -Ttext 0x7C00 -o boot.elf boot.o
$OBJCOPY -O binary boot.elf boot.bin

if [ $? -ne 0 ]; then
    echo "❌ Ошибка сборки boot.S"
    exit 1
fi

echo "🔨 Сборка точки входа..."
$GCC -m32 -ffreestanding -fno-pie -c kernel_entry.S -o kernel_entry.o

echo "🔨 Сборка кучи..."
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c kmalloc.c -o kmalloc.o

echo "🔨 Сборка ядра..."
$GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -c kernel.c -o kernel.o

echo "🔗 Линковка ядра..."
$LD -m elf_i386 -T linker.ld -o kernel.elf kernel_entry.o kmalloc.o kernel.o
$LD -m elf_i386 -T linker.ld --oformat binary -o kernel.bin kernel_entry.o kmalloc.o kernel.o

if [ $? -ne 0 ]; then
    echo "❌ Ошибка линковки"
    exit 1
fi

echo "💾 Создание образа Colibri OS (.cos)..."
dd if=/dev/zero of=colibri.cos bs=512 count=2880 2>/dev/null
dd if=boot.bin of=colibri.cos bs=512 count=1 conv=notrunc 2>/dev/null
dd if=kernel.bin of=colibri.cos bs=512 seek=1 conv=notrunc 2>/dev/null

echo "📏 Размер boot.bin:"
ls -lh boot.bin
echo "📦 Размер kernel.bin:"
ls -lh kernel.bin
echo "📦 Образ:"
ls -lh colibri.cos

echo "🚀 Запуск QEMU..."
qemu-system-i386 -drive file=colibri.cos,format=raw,if=floppy,snapshot=on -net none -vga std