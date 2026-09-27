#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "utils.h"   /* ← ЭТА СТРОКА */

typedef struct {
    uint32_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
} FB_Info;

extern FB_Info fb_info;
extern int fb_enabled;

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_getpixel(uint32_t x, uint32_t y);
void fb_clear(uint32_t color);
void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_rect_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color);

#endif