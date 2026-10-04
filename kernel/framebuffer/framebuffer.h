#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../utils/utils.h"

/* ============================================
   Framebuffer (Bochs VBE / LFB)
   ============================================ */

typedef struct {
    uint32_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
} FB_Info;

#define FB_BLACK        0x000000
#define FB_WHITE        0xFFFFFF
#define FB_RED          0xFF0000
#define FB_GREEN        0x00FF00
#define FB_BLUE         0x0000FF
#define FB_CYAN         0x00FFFF
#define FB_MAGENTA      0xFF00FF
#define FB_YELLOW       0xFFFF00

#define FB_DESKTOP_BG   0x2A4A7A
#define FB_PANEL_BG     0x1A2A3A
#define FB_WINDOW_BG    0x354A5F
#define FB_WINDOW_TITLE 0x4A6B8A
#define FB_TEXT         0xFFFFFF
#define FB_TEXT_DARK    0x000000

extern FB_Info fb_info;
extern int     fb_enabled;

void fb_init_bochs(void);
void fb_init_from_multiboot(unsigned int mbi_addr);

void     fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_getpixel(uint32_t x, uint32_t y);
void     fb_clear(uint32_t color);

void fb_rect     (uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_rect_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_line     (uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color);

/* Обычный вывод в VGA-совместимый framebuffer */
/* Обычный вывод в VGA-совместимый framebuffer */
void fb_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
void fb_print(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
void fb_print_dec(int n, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);

/* Вывод прямо в буфер (для double buffering) */
void fb_putchar_to_buffer(uint32_t* buf, int bw, int bh,
                          char c, int x, int y,
                          uint32_t fg, uint32_t bg);
void fb_print_to_buffer(uint32_t* buf, int bw, int bh,
                        const char* str, int x, int y,
                        uint32_t fg, uint32_t bg);

#endif