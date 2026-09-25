.section .text
.global _start
.extern kernel_main
.global vbe_mode_ptr

.code32
_start:
    cli

    movl %ebx, vbe_mode_ptr

    movw $0x10, %ax
    .byte 0x8E, 0xD8
    .byte 0x8E, 0xC0
    .byte 0x8E, 0xE0
    .byte 0x8E, 0xE8
    .byte 0x8E, 0xD0

    movl $0x90000, %esp
    movl $0x90000, %ebp

    call kernel_main

1:
    hlt
    jmp 1b

.section .data
.global vbe_mode_ptr
vbe_mode_ptr:
    .long 0