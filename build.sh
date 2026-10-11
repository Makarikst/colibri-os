#!/bin/bash
set -e

GCC=x86_64-elf-gcc
LD=x86_64-elf-ld
FLAGS="-m32 -ffreestanding -nostdlib -fno-pic -fno-stack-protector -fno-builtin -Wall -I. -mno-sse -mno-sse2 -mno-mmx -mno-avx -msoft-float"

echo "=== Kernel entry ==="
$GCC $FLAGS -c kernel_entry.S -o kernel_entry.o

echo "=== GraphTool ==="
$GCC $FLAGS -c kernel/graphtool/graphtool.c -o graphtool.o

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

echo "=== PCI ==="
$GCC $FLAGS -c kernel/net/pci.c -o pci.o

echo "=== Font ==="
$GCC $FLAGS -c kernel/framebuffer/font8x16.c -o font8x16.o

echo "=== NanoDesk ==="
$GCC $FLAGS -c kernel/nano_desk/nano_desk.c -o nano_desk.o

echo "=== Filer ==="
$GCC $FLAGS -c kernel/filer/filer.c -o filer.o

echo "=== E1000 ==="
$GCC $FLAGS -c kernel/net/e1000.c -o e1000.o

echo "=== NET ==="
$GCC $FLAGS -c kernel/net/net.c -o net.o

echo "=== ARP ==="
$GCC $FLAGS -c kernel/net/arp.c -o arp.o

echo "=== IP ==="
$GCC $FLAGS -c kernel/net/ip.c -o ip.o

echo "=== ICMP ==="
$GCC $FLAGS -c kernel/net/icmp.c -o icmp.o

echo "=== TCP ==="
$GCC $FLAGS -c kernel/net/tcp.c -o tcp.o

echo "=== UDP ==="
$GCC $FLAGS -c kernel/net/udp.c -o udp.o

echo "=== DNS ==="
$GCC $FLAGS -c kernel/net/dns.c -o dns.o

echo "=== HTTP ==="
$GCC $FLAGS -c kernel/net/http.c -o http.o

echo "=== HTML ==="
$GCC $FLAGS -c kernel/browser/html.c -o html.o

echo "=== HTML JS ==="
$GCC $FLAGS -c kernel/browser/html_js.c -o html_js.o

echo "=== Browser ==="
$GCC $FLAGS -c kernel/browser/browser.c -o browser.o

echo "=== CX ==="
$GCC $FLAGS -c kernel/cx/cx.c -o cx.o

echo "=== Link ==="
$LD -m elf_i386 -T linker.ld -o colibri.cos \
    --start-group \
    kernel_entry.o kernel.o vga.o kbd.o utils.o kmalloc.o framebuffer.o \
    menupkm.o startmenu.o graphtool.o pci.o \
    font8x16.o fs.o commands.o nano.o cpe.o shell.o desktop.o mouse.o wm.o \
    term.o filer.o nano_desk.o \
    e1000.o net.o arp.o ip.o icmp.o tcp.o udp.o dns.o http.o \
    html.o html_js.o browser.o cx.o \
    --end-group

echo "colibri.cos:"
ls -l colibri.cos

echo "=== Run ==="
qemu-system-i386 -kernel colibri.cos -m 64M -vga std \
    -netdev user,id=n0 -device e1000,netdev=n0