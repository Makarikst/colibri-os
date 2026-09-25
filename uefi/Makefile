# ============================================
# Colibri OS — UEFI Makefile
# ============================================

GCC = x86_64-elf-gcc
LD  = x86_64-elf-ld
OBJCOPY = /opt/homebrew/opt/mingw-w64/bin/x86_64-w64-mingw32-objcopy

GNUEFI_DIR = gnu-efi
GNUEFI_INC = $(GNUEFI_DIR)/inc
GNUEFI_LIB = $(GNUEFI_DIR)/lib

CFLAGS = -m64 -ffreestanding -fno-stack-protector -mno-red-zone \
         -fshort-wchar -fno-builtin -fPIC \
         -I$(GNUEFI_INC) -I$(GNUEFI_INC)/x86_64 -I$(GNUEFI_INC)/protocol

all: bootx64.efi kernel.efi

# ---------- UEFI загрузчик ----------
bootx64.o: efi/boot/bootx64.c
	$(GCC) $(CFLAGS) -c $< -o $@

bootx64.so: bootx64.o
	$(LD) -shared -Bsymbolic -L$(GNUEFI_LIB) -T $(GNUEFI_LIB)/elf_x86_64_efi.lds \
	    $(GNUEFI_LIB)/crt0-efi-x86_64.o $< -o $@ -lefi -lgnuefi

bootx64.efi: bootx64.so
	$(OBJCOPY) -O efi-app-x86-64 \
	    -j .text -j .sdata -j .data -j .rodata \
	    -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc \
	    $< $@

# ---------- Ядро ----------
kernel_entry.o: kernel/kernel_entry.S
	$(GCC) -m64 -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -c $< -o $@

kmalloc.o: kernel/kmalloc.c
	$(GCC) -m64 -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -c $< -o $@

kernel.o: kernel/kernel.c
	$(GCC) -m64 -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -c $< -o $@

kernel.efi: kernel_entry.o kernel.o kmalloc.o
	$(LD) -m elf_x86_64 -T kernel/linker.ld -o $@ $^

clean:
	rm -f *.o *.so *.efi