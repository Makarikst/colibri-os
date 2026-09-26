<h1>Colibri OS</h1>
<h3>v0.8 in 26 Sep 2026</h3>

![Version](https://img.shields.io/badge/version-0.8-blue)
![Status](https://img.shields.io/badge/status-stable-green)
![Platform](https://img.shields.io/badge/platform-i386-lightgrey)
![License](https://img.shields.io/badge/license-MIT-yellow)

**Colibri OS** is a small hobby operating system for the **i386** architecture, written in C and assembly. It boots via **multiboot**, runs in **32-bit protected mode**, and features its own shell, in-memory filesystem, and a set of built-in commands.

## Features

- **VGA text mode** 80x25 with colors (16 palettes)
- **Interactive shell** with prompt `colibri:/users/alpha>`
- **In-memory filesystem** (files and directories)
- **Built-in calculator** with `+ - * / % ^`
- **Math**: `sqrt`, `pow`, `gcd`, `lcm`, `prime`, `rand`, `hex`, `bin`
- **Environment variables** and **command aliases**
- **Stubs** for processes and networking
- **Multiboot boot** — works with QEMU, GRUB, VirtualBox

## Quick Start

### Requirements

- `x86_64-elf-gcc` — cross-compiler
- `x86_64-elf-ld` — linker
- `qemu-system-i386` — emulator

### Build and Run

    git clone https://github.com/Makarikst/colibri-os.git
    cd colibri-os
    ./build.sh

The build produces **`colibri.cos`** — an ELF kernel image. QEMU runs it automatically.

Manual run:

    qemu-system-i386 -kernel colibri.cos -m 16M -net none -vga std

## Project Structure

    colibri-os/
    ├── build.sh           # Build and run script
    ├── linker.ld          # Linker script (kernel at 0x100000)
    ├── kernel_entry.S     # Entry point, multiboot header
    ├── kernel.c           # Kernel: VGA, shell, FS, commands
    ├── kmalloc.c/.h       # Heap allocator
    ├── utils.c/.h         # Utilities: strings, numbers, RTC, ports
    └── colibri.cos        # Compiled ELF image

## Shell Commands

Type `help` in the shell for the full list.

### System

| Command | Description |
|---------|-------------|
| `help` | Show all commands |
| `ver` | System version |
| `banner` | Show banner |
| `clear` | Clear screen |
| `date` | Date and time |
| `mem` | Memory info |
| `heap` | Heap statistics |
| `history` | Command history |
| `reboot` | Restart |
| `shutdown` | Power off |

### Files

| Command | Description |
|---------|-------------|
| `ls [path]` | List files |
| `pwd` | Current directory |
| `cd <path>` | Change directory |
| `mkdir X` | Create directory |
| `touch X` | Create file |
| `rm X` | Delete |
| `cat <file>` | Show file |
| `write <f> <t>` | Write text |
| `cp <src> <dst>` | Copy |
| `mv <src> <dst>` | Move |
| `tree` | File tree |
| `stat <file>` | File info |
| `wc <file>` | Lines / words / bytes |
| `grep <pat> <file>` | Search in file |

### Math

| Command | Description |
|---------|-------------|
| `calc <expr>` | Calculator: `+ - * / % ^` |
| `sqrt <n>` | Square root |
| `pow <a> <b>` | Power |
| `prime <n>` | Primality test |
| `gcd <a> <b>` | GCD |
| `lcm <a> <b>` | LCM |
| `hex <n>` | To hexadecimal |
| `bin <n>` | To binary |

### Environment

| Command | Description |
|---------|-------------|
| `env` | Show variables |
| `setenv K=V` | Set variable |
| `alias X=Y` | Create alias |
| `unalias X` | Remove alias |

## How It Works

### Boot

The kernel is a **multiboot-compliant ELF**. QEMU or GRUB reads the multiboot header in the first 8 KB and jumps to `_start`.

    .section .multiboot, "a"
        .long 0x1BADB002    # magic
        .long 0x00000003    # flags
        .long -(MAGIC + FLAGS)

### Memory

- Kernel: `0x100000` (1 MB)
- Stack: `0x9F000`
- Heap: `0x200000` (2 MB)

### Filesystem

In-memory FS: an array of `FsObject` with type (`FILE` / `DIR`), name, parent, and up to 4 KB of data per file.

## Roadmap

- [x] VGA output and banner
- [x] Interactive shell
- [x] In-memory filesystem
- [x] Calculator and math
- [x] Environment variables and aliases
- [ ] Screen scrollback with arrows
- [ ] Real process scheduling
- [ ] Full keyboard driver
- [ ] ATA PIO disk driver
- [ ] Networking (NE2000 / RTL8139)
- [ ] Bootable ISO with GRUB

## Running on Real Hardware

Build `colibri.cos` and put it on a USB stick with GRUB. Example `grub.cfg`:

    menuentry "Colibri OS" {
        multiboot /boot/colibri.cos
        boot
    }

Install GRUB on USB:

    sudo grub-install --target=i386-pc --boot-directory=/mnt/usb/boot /dev/sdX

## License

MIT

## Author

**Asde LLC** — hobby project, 2026.

Issues, bugs, ideas — open an [issue](https://github.com/Makarikst/colibri-os/issues).

---

*Colibri is the smallest bird in the world. Just like this OS.*
