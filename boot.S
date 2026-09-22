/* ============================================
   Colibri OS — Bootloader (MBR, 16-bit)
   ============================================ */

.code16
.section .text
.global _start

.equ KERNEL_OFFSET,  0x1000
.equ KERNEL_SECTORS, 40

_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp

    /* ТЕСТ 1: пишем 'A' в VGA напрямую */
    movw $0xB800, %ax
    movw %ax, %es
    xorw %di, %di
    movb $'A', %es:(%di)
    movb $0x0F, %es:1(%di)

    /* Возвращаем es = 0 */
    xorw %ax, %ax
    movw %ax, %es

    movb %dl, boot_drive

    movw $msg_loading, %si
    call print_string

    sti

    /* Загружаем ядро */
    movw $KERNEL_OFFSET, %bx
    movb $KERNEL_SECTORS, %dh
    movb boot_drive, %dl
    call load_kernel

    cli

    /* ТЕСТ 2: пишем 'B' в VGA после загрузки ядра */
    movw $0xB800, %ax
    movw %ax, %es
    movw $4, %di
    movb $'B', %es:(%di)
    movb $0x0F, %es:1(%di)

    xorw %ax, %ax
    movw %ax, %es

    movw $msg_jumping, %si
    call print_string

    /* ========================================
       ПЕРЕХОД В PROTECTED MODE
       ======================================== */
    cli
    lgdt gdt_descriptor

    /* ТЕСТ 3: пишем 'C' после lgdt */
    movw $0xB800, %ax
    movw %ax, %es
    movw $8, %di
    movb $'C', %es:(%di)
    movb $0x0F, %es:1(%di)

    xorw %ax, %ax
    movw %ax, %es

    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0

    /* Дальний прыжок в 32-битный сегмент */
    .byte 0x66, 0xEA
    .long 0x00001000
    .word 0x0008

/* ============================================
   16-битные функции
   ============================================ */
print_string:
    movb $0x0E, %ah
1:
    lodsb
    testb %al, %al
    jz 2f
    int $0x10
    jmp 1b
2:
    ret

load_kernel:
    movb $0x02, %ah
    movb %dh, %al
    movb $0x00, %ch
    movb $0x00, %dh
    movb $0x02, %cl
    int $0x13
    jc disk_error
    ret

disk_error:
    movw $msg_disk_error, %si
    call print_string
    jmp .

/* ============================================
   GDT
   ============================================ */
.align 8
gdt_start:
    .quad 0x0000000000000000
gdt_code:
    .quad 0x00CF9A000000FFFF
gdt_data:
    .quad 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start

/* ============================================
   Данные
   ============================================ */
boot_drive:     .byte 0
msg_loading:    .asciz "Colibri: loading kernel...\r\n"
msg_jumping:    .asciz "Colibri: jumping to kernel...\r\n"
msg_disk_error: .asciz "Colibri: disk read error!\r\n"

.org 510
.word 0xAA55