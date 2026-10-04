#ifndef VGA_H
#define VGA_H

#include "../utils/utils.h"

/* ============================================
   VGA текст 80x25, видеопамять 0xB8000
   + scroll buffer 500 строк
   ============================================ */

#define VGA_MEMORY    0xB8000
#define VGA_WIDTH     80
#define VGA_HEIGHT    25
#define SCROLL_LINES  500
#define SCROLL_COLS   80
#define VIEW_LINES    25

/* Цвета VGA */
#define VGA_BLACK         0
#define VGA_BLUE          1
#define VGA_GREEN         2
#define VGA_CYAN          3
#define VGA_RED           4
#define VGA_MAGENTA       5
#define VGA_BROWN         6
#define VGA_LIGHT_GRAY    7
#define VGA_DARK_GRAY     8
#define VGA_LIGHT_BLUE    9
#define VGA_LIGHT_GREEN   10
#define VGA_LIGHT_CYAN    11
#define VGA_LIGHT_RED     12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW        14
#define VGA_WHITE         15

/* --- Инициализация --- */
void vga_init(void);

/* --- Вывод --- */
void vga_putchar(char c);
extern void (*vga_output_target)(char c);
void vga_print(const char* str);
void vga_print_color(const char* str, unsigned char col);
void vga_print_dec(int n);
void vga_print_hex32(unsigned int n);

/* --- Управление --- */
void vga_clear(void);
void vga_banner(void);
void vga_set_color(unsigned char col);

/* --- Скролл --- */
void vga_scroll_up(void);
void vga_scroll_down(void);

#endif