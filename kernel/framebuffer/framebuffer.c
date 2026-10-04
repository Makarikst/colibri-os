#include "framebuffer.h"
#include "font8x16.h"

/* ============================================
   Состояние
   ============================================ */
FB_Info fb_info = {0, 0, 0, 0, 0};
int     fb_enabled = 0;

/* ============================================
   Bochs VBE
   ============================================ */
static void bochs_write(uint16_t index, uint16_t value) {
    outw(0x1CE, index);
    outw(0x1CF, value);
}

static uint16_t bochs_read(uint16_t index) {
    outw(0x1CE, index);
    return inw(0x1CF);
}

void fb_init_bochs(void) {
    uint16_t id = bochs_read(0);
    if ((id & 0xFFF0) != 0xB0C0) {
        fb_enabled = 0;
        return;
    }

    bochs_write(4, 0x00);
    bochs_write(1, 1024);
    bochs_write(2, 768);
    bochs_write(3, 32);
    bochs_write(6, 1024);
    bochs_write(7, 768);
    bochs_write(8, 0);
    bochs_write(9, 0);
    bochs_write(4, 0x41);

    fb_info.addr   = 0xFD000000;
    fb_info.width  = 1024;
    fb_info.height = 768;
    fb_info.pitch  = 1024 * 4;
    fb_info.bpp    = 32;
    fb_enabled     = 1;
}

void fb_init_from_multiboot(unsigned int mbi_addr) {
    if (mbi_addr == 0) { fb_enabled = 0; return; }
    unsigned int* mbi = (unsigned int*) mbi_addr;
    unsigned int flags = mbi[0];
    if (!(flags & (1 << 12))) { fb_enabled = 0; return; }
    fb_info.addr   = mbi[22];
    fb_info.pitch  = mbi[23];
    fb_info.width  = mbi[24];
    fb_info.height = mbi[25];
    fb_info.bpp    = *(unsigned char*)(mbi_addr + 104);
    fb_enabled = 1;
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

void fb_clear(uint32_t color) {
    if (!fb_enabled) return;
    for (uint32_t y = 0; y < fb_info.height; y++)
        for (uint32_t x = 0; x < fb_info.width; x++)
            fb_putpixel(x, y, color);
}

/* ============================================
   Фигуры
   ============================================ */
void fb_rect_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_enabled) return;
    for (uint32_t dy = 0; dy < h; dy++)
        for (uint32_t dx = 0; dx < w; dx++)
            fb_putpixel(x + dx, y + dy, color);
}

void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_enabled) return;
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
    if (!fb_enabled) return;
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

/* ============================================
   Текст в VGA-framebuffer
   ============================================ */
void fb_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    if (!fb_enabled) return;
    unsigned char ch = (unsigned char)c;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            fb_putpixel(x + col, y + row, color);
        }
    }
}

void fb_print(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    uint32_t cx = x;
    while (*str) {
        if (*str == '\n') { cx = x; y += 16; }
        else { fb_putchar(*str, cx, y, fg, bg); cx += 8; }
        str++;
    }
}

void fb_print_dec(int n, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    char buf[16];
    itoa(n, buf, 10);
    fb_print(buf, x, y, fg, bg);
}

/* ============================================
   Текст прямо в буфер (double buffering)
   ============================================ */
void fb_putchar_to_buffer(uint32_t* buf, int bw, int bh,
                          char c, int x, int y,
                          uint32_t fg, uint32_t bg)
{
    unsigned char ch = (unsigned char)c;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= bw) continue;
            if (py < 0 || py >= bh) continue;
            buf[py * bw + px] = color;
        }
    }
}

void fb_print_to_buffer(uint32_t* buf, int bw, int bh,
                        const char* str, int x, int y,
                        uint32_t fg, uint32_t bg)
{
    int cx = x;
    int cy = y;
    while (*str) {
        if (*str == '\n') { cx = x; cy += 16; }
        else {
            fb_putchar_to_buffer(buf, bw, bh, *str, cx, cy, fg, bg);
            cx += 8;
        }
        str++;
    }
}