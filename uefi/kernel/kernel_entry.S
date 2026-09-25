/* ============================================
   Colibri OS — Kernel entry point (x86_32)
   Вызывается из UEFI-загрузчика после перехода в PM.
   BootInfo лежит по адресу 0x7000.
   ============================================ */

.section .text
.global _start
.extern kernel_main

.code32
_start:
    cli

    /* Стек */
    movl $0x900000, %esp
    movl $0x900000, %ebp

    /* Указатель на BootInfo = 0x7000 (передадим как аргумент) */
    movl $0x7000, %edi

    /* Вызов kernel_main(BootInfo*) */
    pushl %edi
    call kernel_main

1:
    hlt
    jmp 1b