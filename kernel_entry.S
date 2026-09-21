/* ============================================
   Colibri OS — точка входа 32-битного ядра
   ============================================ */

.section .text
.global _start
.extern kernel_main

.code32
_start:
    cli

    /* Устанавливаем сегменты данных
       GAS в .code32 кодирует mov ax, ds как mov eax, ds,
       поэтому кодируем вручную 16-битными префиксами. */

    movw $0x10, %ax

    .byte 0x8E, 0xD8        /* mov ds, ax */
    .byte 0x8E, 0xC0        /* mov es, ax */
    .byte 0x8E, 0xE0        /* mov fs, ax */
    .byte 0x8E, 0xE8        /* mov gs, ax */
    .byte 0x8E, 0xD0        /* mov ss, ax */

    movl $0x90000, %esp
    movl $0x90000, %ebp

    /* Тест VGA: пишем 'OK' */
    movl $0xB8000, %edi
    movw $0x0F4F, (%edi)
    movw $0x0F4B, 2(%edi)

    call kernel_main

hang:
    hlt
    jmp hang