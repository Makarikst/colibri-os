# ============================================
# Colibri OS - Multiboot entry
# ============================================

.set MAGIC,    0x1BADB002
.set FLAGS,    0x00000003
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

.section .text
.global _start
.extern kernel_main
.extern _bss_start
.extern _bss_end

.code32
_start:
    cli
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    movl $0x9F000, %esp
    movl $0x9F000, %ebp

    # Clear BSS
    movl $_bss_start, %edi
    movl $_bss_end, %ecx
    subl %edi, %ecx
    xorl %eax, %eax
    rep stosb

    call kernel_main

hang:
    hlt
    jmp hang