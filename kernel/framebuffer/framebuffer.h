#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../utils/utils.h"

/* ============================================
   Framebuffer (VBE / LFB)
   ============================================ */

typedef struct {
    uint32_t addr;      /* физический адрес LFB */
    uint32_t width;     /* ширина в пикселях */
    uint32_t height;    /* высота в пикселях */
    uint32_t pitch;     /* байт на строку */
    uint8_t  bpp;       /* бит на пиксель */
} FB_Info;

/* Цвета RGB888 */
#define FB_BLACK        0x000000
#define FB_WHITE        0xFFFFFF
#define FB_RED          0xFF0000
#define FB_GREEN        0x00FF00
#define FB_BLUE         0x0000FF
#define FB_CYAN         0x00FFFF
#define FB_MAGENTA      0xFF00FF
#define FB_YELLOW       0xFFFF00
#define FB_LIGHT_GREEN  0x00FF80
#define FB_DARK_GRAY    0x404040
#define FB_LIGHT_GRAY   0xC0C0C0

/* Глобальные переменные */
extern FB_Info fb_info;
extern int     fb_enabled;

/* ============================================
   Инициализация
   ============================================ */
void fb_init_from_multiboot(unsigned int mbi_addr);
void fb_init(void);

/* ============================================
   Рисование
   ============================================ */
void     fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_getpixel(uint32_t x, uint32_t y);
void     fb_clear(uint32_t color);

/* ============================================
   Фигуры
   ============================================ */
void fb_rect     (uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_rect_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_line     (uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color);

#endif