#!/bin/bash
set -e

GCC=x86_64-elf-gcc
LD=x86_64-elf-ld
FLAGS="-m32 -ffreestanding -nostdlib -fno-pic -fno-stack-protector -fno-builtin -Wall -I. -mno-sse -mno-sse2 -mno-mmx -mno-avx -msoft-float"

echo "=== Kernel entry ==="
$GCC $FLAGS -c kernel_entry.S -o kernel_entry.o

echo "=== VGA ==="
$GCC $FLAGS -c kernel/vga/vga.c -o vga.o

echo "=== Kbd ==="
$GCC $FLAGS -c kernel/kbd/kbd.c -o kbd.o

echo "=== Utils ==="
$GCC $FLAGS -c kernel/utils/utils.c -o utils.o

echo "=== Heap ==="
$GCC $FLAGS -c kernel/kmalloc/kmalloc.c -o kmalloc.o

echo "=== Framebuffer ==="
$GCC $FLAGS -c kernel/framebuffer/framebuffer.c -o framebuffer.o

echo "=== Menu (PKM) ==="
$GCC $FLAGS -c kernel/menupkm/menupkm.c -o menupkm.o

echo "=== StartMenu ==="
$GCC $FLAGS -c kernel/startmenu/startmenu.c -o startmenu.o

echo "=== FS ==="
$GCC $FLAGS -c kernel/fs/fs.c -o fs.o

echo "=== Commands ==="
$GCC $FLAGS -c kernel/commands/commands.c -o commands.o

echo "=== Nano ==="
$GCC $FLAGS -c kernel/nano/nano.c -o nano.o

echo "=== CPE ==="
$GCC $FLAGS -c kernel/cpe/cpe.c -o cpe.o

echo "=== Shell ==="
$GCC $FLAGS -c kernel/shell/shell.c -o shell.o

echo "=== Kernel ==="
$GCC $FLAGS -c kernel.c -o kernel.o

echo "=== Mouse ==="
$GCC $FLAGS -c kernel/mouse/mouse.c -o mouse.o

echo "=== Desktop ==="
$GCC $FLAGS -c kernel/desktop/desktop.c -o desktop.o

echo "=== WM ==="
$GCC $FLAGS -c kernel/wm/wm.c -o wm.o

echo "=== Term ==="
$GCC $FLAGS -c kernel/term/term.c -o term.o

echo "=== Font ==="
$GCC $FLAGS -c kernel/framebuffer/font8x16.c -o font8x16.o

echo "=== NanoDesk ==="
$GCC $FLAGS -c kernel/nano_desk/nano_desk.c -o nano_desk.o

echo "=== Filer ==="
$GCC $FLAGS -c kernel/filer/filer.c -o filer.o

echo "=== Link ==="
$LD -m elf_i386 -T linker.ld -o colibri.cos \
    --start-group \
    kernel_entry.o kernel.o vga.o kbd.o utils.o kmalloc.o framebuffer.o menupkm.o startmenu.o \
    font8x16.o fs.o commands.o nano.o cpe.o shell.o desktop.o mouse.o wm.o term.o filer.o nano_desk.o \
    --end-group

echo "colibri.cos:"
ls -l colibri.cos

echo "=== Run ==="
qemu-system-i386 -kernel colibri.cos -m 64M -vga std