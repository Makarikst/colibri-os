#include "framebuffer.h"

/* ============================================
   Состояние
   ============================================ */
FB_Info fb_info = {0, 0, 0, 0, 0};
int     fb_enabled = 0;

/* ============================================
   Инициализация из multiboot
   ============================================ */
void fb_init_from_multiboot(unsigned int mbi_addr) {
    if (mbi_addr == 0) { fb_enabled = 0; return; }

    unsigned int* mbi = (unsigned int*) mbi_addr;
    unsigned int flags = mbi[0];

    /* bit 12 = framebuffer info present */
    if (!(flags & (1 << 12))) { fb_enabled = 0; return; }

    /* Multiboot 1: framebuffer поля начиная с offset 88 */
    fb_info.addr   = mbi[22];   /* framebuffer_addr */
    fb_info.pitch  = mbi[23];   /* framebuffer_pitch */
    fb_info.width  = mbi[24];   /* framebuffer_width */
    fb_info.height = mbi[25];   /* framebuffer_height */
    fb_info.bpp    = *(unsigned char*)(mbi_addr + 104);

    fb_enabled = 1;
}

/* ============================================
   Инициализация вручную (для QEMU -vga std)
   ============================================ */
void fb_init(void) {
    fb_info.addr   = 0xFD000000;
    fb_info.width  = 1024;
    fb_info.height = 768;
    fb_info.pitch  = 1024 * 4;
    fb_info.bpp    = 32;
    fb_enabled     = 1;
}

/* ============================================
   Пиксели
   ============================================ */
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_enabled) return;
    if (x >= fb_info.width || y >= fb_info.height) return;
    uint32_t* pixel = (uint32_t*)(fb_info.addr + y * fb_info.pitch + x * 4);
    *pixel = color;
}

uint32_t fb_getpixel(uint32_t x, uint32_t y) {
    if (!fb_enabled) return 0;
    if (x >= fb_info.width || y >= fb_info.height) return 0;
    return *(uint32_t*)(fb_info.addr + y * fb_info.pitch + x * 4);
}

/* ============================================
   Очистка
   ============================================ */
void fb_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb_info.height; y++)
        for (uint32_t x = 0; x < fb_info.width; x++)
            fb_putpixel(x, y, color);
}

/* ============================================
   Прямоугольники
   ============================================ */
void fb_rect_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t dy = 0; dy < h; dy++)
        for (uint32_t dx = 0; dx < w; dx++)
            fb_putpixel(x + dx, y + dy, color);
}

void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < w; i++) {
        fb_putpixel(x + i, y, color);
        fb_putpixel(x + i, y + h - 1, color);
    }
    for (uint32_t i = 0; i < h; i++) {
        fb_putpixel(x, y + i, color);
        fb_putpixel(x + w - 1, y + i, color);
    }
}

/* ============================================
   Линия (алгоритм Брезенхэма)
   ============================================ */
void fb_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        fb_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}