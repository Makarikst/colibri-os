#!/bin/bash
# ============================================
# Colibri OS — UEFI build script
# ============================================

set -e

cd "$(dirname "$0")"

echo "🔨 Сборка UEFI-версии..."
make clean
make

if [ ! -f bootx64.efi ] || [ ! -f kernel.efi ]; then
    echo "❌ Ошибка сборки"
    exit 1
fi

echo "💾 Создание FAT-образа..."
dd if=/dev/zero of=colibri-uefi.img bs=512 count=2880 2>/dev/null

# Форматируем FAT12 (mtools)
export MTOOLS_SKIP_CHECK=1
mformat -i colibri-uefi.img -f 1440 ::

# Создаём папки EFI/BOOT
mmd -i colibri-uefi.img ::/EFI
mmd -i colibri-uefi.img ::/EFI/BOOT

# Копируем файлы
mcopy -i colibri-uefi.img bootx64.efi ::/EFI/BOOT/BOOTX64.EFI
mcopy -i colibri-uefi.img kernel.efi ::/kernel.efi

echo "✅ Образ: colibri-uefi.img"

echo "🚀 Запуск QEMU с UEFI..."
qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file=/opt/homebrew/Cellar/qemu/11.1.0/share/qemu/edk2-x86_64-code.fd \
    -drive format=raw,file=colibri-uefi.img \
    -net none \
    -vga std