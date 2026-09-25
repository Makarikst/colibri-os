.code16
.global _start

.equ KERNEL_SEG,     0x1000
.equ KERNEL_SECTORS, 40
.equ VBE_MODE_ADDR,  0x7200

_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp

    movb %dl, boot_drive

    /* ========================================
       ШАГ 1: Читаем ядро (в текстовом режиме)
       ======================================== */
    movb $'r', %al
    movw $0xE9, %dx
    outb %al, %dx

    movw $KERNEL_SEG, %ax
    movw %ax, %es
    xorw %bx, %bx
    movw $KERNEL_SECTORS, %si

    movb $0x00, %ch
    movb $0x00, %dh
    movb $0x02, %cl

.read_loop:
    movb $0x02, %ah
    movb $1, %al
    movb boot_drive, %dl
    int $0x13
    jc disk_error

    movb $'.', %al
    movw $0xE9, %dx
    outb %al, %dx

    addw $512, %bx
    incb %cl
    cmpb $19, %cl
    jb .no_inc
    movb $1, %cl
    incb %dh
    cmpb $2, %dh
    jb .no_inc
    movb $0, %dh
    incb %ch
.no_inc:

    testw %bx, %bx
    jnz .no_es
    movw %es, %ax
    addw $0x1000, %ax
    movw %ax, %es
.no_es:

    decw %si
    jnz .read_loop

    movb $'R', %al
    movw $0xE9, %dx
    outb %al, %dx

    /* ========================================
       ШАГ 2: VESA (в текстовом режиме)
       ======================================== */
    movw $0x7000, %di
    movw $0x4F00, %ax
    movw $0x0000, %bx
    int $0x10
    cmpw $0x4F, %ax
    jne vesa_fail

    movb $'C', %al
    movw $0xE9, %dx
    outb %al, %dx

    /* Ищем режим */
    movw $0x7000, %si
    movw 14(%si), %si
    cmpw $0x8000, %si
    jb .mode_loop
    movw %si, %ax
    movw $0x7000, %si
    movw %ax, %si

.mode_loop:
    movw (%si), %cx
    cmpw $0xFFFF, %cx
    je try_default_mode

    /* Получаем Mode Info */
    pushw %si
    movw $0x7200, %di
    movw $0x4F01, %ax
    int $0x10
    popw %si

    cmpw $0x4F, %ax
    jne .next_mode

    /* Проверяем: не пустой ли режим */
    movw $0x7200, %di
    cmpw $0, 18(%di)
    je .next_mode
    cmpw $0, 20(%di)
    je .next_mode
    cmpb $0, 25(%di)
    je .next_mode

    /* ========================================
       Нашли режим! Устанавливаем
       ======================================== */
    movw %cx, %bx
    movw $0x4F02, %ax
    orw $0x4000, %bx
    int $0x10
    cmpw $0x4F, %ax
    jne .next_mode

    movb $'S', %al
    movw $0xE9, %dx
    outb %al, %dx
    jmp vesa_ok

.next_mode:
    addw $2, %si
    jmp .mode_loop

try_default_mode:
    /* Пробуем стандартный режим 0x118 (1024x768x24) */
    movw $0x118, %bx
    movw $0x4F02, %ax
    orw $0x4000, %bx
    int $0x10
    cmpw $0x4F, %ax
    jne vesa_fail

    movb $'S', %al
    movw $0xE9, %dx
    outb %al, %dx
    jmp vesa_ok

vesa_fail:
    movb $'V', %al
    movw $0xE9, %dx
    outb %al, %dx
1:  jmp 1b

vesa_ok:
    movb $'J', %al
    movw $0xE9, %dx
    outb %al, %dx

    cli
    lgdt gdt_descriptor

    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0

    movl $VBE_MODE_ADDR, %ebx

    .byte 0x66, 0xEA
    .long 0x00010000
    .word 0x0008

disk_error:
    movb $'D', %al
    movw $0xE9, %dx
    outb %al, %dx
1:  jmp 1b

.align 8
gdt_start:
    .quad 0
gdt_code:
    .quad 0x00CF9A000000FFFF
gdt_data:
    .quad 0x00CF92000000FFFF
gdt_end:
gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start

boot_drive: .byte 0

.org 510
.word 0xAA55