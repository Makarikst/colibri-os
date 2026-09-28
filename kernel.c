/* ============================================
Colibri OS v0.9 — kernel_main
   ============================================ */

#include "kernel/vga/vga.h"
#include "kernel/kbd/kbd.h"
#include "kernel/utils/utils.h"
#include "kernel/kmalloc/kmalloc.h"
#include "kernel/framebuffer/framebuffer.h"
#include "kernel/fs/fs.h"
#include "kernel/commands/commands.h"
#include "kernel/shell/shell.h"

/* ============================================
   Точка входа ядра
   ============================================ */
void kernel_main(void) {

    /* --- Инициализация железа --- */
    vga_init();
    vga_banner();
    kbd_init();

    /* --- Инициализация подсистем --- */
    heap_init();
    fs_init();
    srand(12345);

    /* --- Framebuffer: включаем, если multiboot передал LFB --- */
    extern unsigned int mbi_ptr;
    if (mbi_ptr != 0) {
        fb_init_from_multiboot(mbi_ptr);
        if (fb_enabled) {
            fb_clear(0x000080);
            fb_rect_fill(100, 100, 400, 200, 0xFF0000);
            fb_rect(100, 100, 400, 200, 0xFFFFFF);
        }
    }

    /* --- Оболочка --- */
    shell_run();

    /* --- Если shell_run вышел — стоп --- */
    while (1) cpu_halt();
}