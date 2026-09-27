/* framebuffer.c */

#include "framebuffer.h"
#include "utils.h"

FB_Info fb_info = {0, 0, 0, 0, 0};
int fb_enabled = 0;

/* === PCI helpers — ОБЯЗАТЕЛЬНО ДО fb_init === */
static unsigned int pci_read(unsigned char bus, unsigned char slot,
                              unsigned char func, unsigned char off) {
    unsigned int addr = (1u << 31) | (bus << 16) | (slot << 11)
                      | (func << 8) | (off & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

static void pci_write(unsigned char bus, unsigned char slot,
                       unsigned char func, unsigned char off,
                       unsigned int val) {
    unsigned int addr = (1u << 31) | (bus << 16) | (slot << 11)
                      | (func << 8) | (off & 0xFC);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

/* === fb_init — теперь использует PCI === */
void fb_init(void) {
    unsigned int bus, slot;
    unsigned int addr = 0;

    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            unsigned int vendev = pci_read(bus, slot, 0, 0x00);
            if ((vendev & 0xFFFF) == 0xFFFF) continue;

            unsigned int cls = (pci_read(bus, slot, 0, 0x08) >> 24) & 0xFF;
            if (cls != 0x03) continue;

            unsigned int cmd = pci_read(bus, slot, 0, 0x04);
            cmd |= 0x0002;
            pci_write(bus, slot, 0, 0x04, cmd);

            unsigned int bar0 = pci_read(bus, slot, 0, 0x10);
            addr = bar0 & 0xFFFFFFF0;
            if (addr) goto found;
        }
    }

    fb_enabled = 0;
    return;

found:
    fb_info.addr   = addr;
    fb_info.width  = 1024;
    fb_info.height = 768;
    fb_info.pitch  = 1024 * 4;
    fb_info.bpp    = 32;
    fb_enabled     = 1;

    /* Отладка: печатаем адрес в углу VGA */
    volatile unsigned char* vga = (unsigned char*) 0xB8000;
    const char* hex = "0123456789abcdef";
    vga[160] = '0'; vga[161] = 0x0E;
    vga[162] = 'x'; vga[163] = 0x0E;
    for (int i = 0; i < 8; i++) {
        vga[164 + i * 2] = hex[(addr >> (28 - i * 4)) & 0xF];
        vga[164 + i * 2 + 1] = 0x0E;
    }
}

/* ... остальные fb_* функции ... */

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

void fb_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb_info.height; y++)
        for (uint32_t x = 0; x < fb_info.width; x++)
            fb_putpixel(x, y, color);
}

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