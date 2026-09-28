#include "vga.h"

/* ============================================
   Состояние
   ============================================ */
static unsigned char color = (VGA_BLACK << 4) | VGA_WHITE;

static unsigned char scroll_buf[SCROLL_LINES][SCROLL_COLS];
static unsigned char scroll_col[SCROLL_LINES][SCROLL_COLS];
static int scroll_total = 0;
static int scroll_offset = 0;
static int sb_line = 0;
static int sb_col  = 0;

/* ============================================
   Инициализация
   ============================================ */
void vga_init(void) {
    for (int i = 0; i < SCROLL_LINES; i++)
        for (int j = 0; j < SCROLL_COLS; j++) {
            scroll_buf[i][j] = ' ';
            scroll_col[i][j] = color;
        }
    scroll_total = 0;
    scroll_offset = 0;
    sb_line = 0;
    sb_col = 0;
    vga_clear();
}

/* ============================================
   Отрисовка экрана из буфера
   ============================================ */
static void scroll_redraw(void) {
    unsigned char* vga = (unsigned char*) VGA_MEMORY;
    int start = scroll_total - VIEW_LINES - scroll_offset;
    if (start < 0) start = 0;

    for (int i = 0; i < VIEW_LINES; i++) {
        int src = start + i;
        for (int j = 0; j < SCROLL_COLS; j++) {
            char c = (src >= 0 && src < scroll_total) ? scroll_buf[src][j] : ' ';
            unsigned char col = (src >= 0 && src < scroll_total) ? scroll_col[src][j] : color;
            int offset = (i * SCROLL_COLS + j) * 2;
            vga[offset] = c;
            vga[offset + 1] = col;
        }
    }
}

/* ============================================
   Запись в буфер
   ============================================ */
static void scroll_buffer_putchar(char c) {
    if (c == '\n') {
        sb_line++;
        sb_col = 0;
        if (sb_line >= SCROLL_LINES) {
            for (int i = 0; i < SCROLL_LINES - 1; i++)
                for (int j = 0; j < SCROLL_COLS; j++) {
                    scroll_buf[i][j] = scroll_buf[i+1][j];
                    scroll_col[i][j] = scroll_col[i+1][j];
                }
            sb_line = SCROLL_LINES - 1;
            for (int j = 0; j < SCROLL_COLS; j++) {
                scroll_buf[sb_line][j] = ' ';
                scroll_col[sb_line][j] = color;
            }
        }
        if (sb_line + 1 > scroll_total) {
            scroll_total = sb_line + 1;
            if (scroll_total > SCROLL_LINES) scroll_total = SCROLL_LINES;
        }
        return;
    }
    if (c == '\b') {
        if (sb_col > 0) {
            sb_col--;
            scroll_buf[sb_line][sb_col] = ' ';
            scroll_col[sb_line][sb_col] = color;
        }
        return;
    }
    if ((unsigned char)c < 32) return;
    if (sb_col >= SCROLL_COLS) {
        sb_col = 0;
        sb_line++;
        if (sb_line >= SCROLL_LINES) {
            for (int i = 0; i < SCROLL_LINES - 1; i++)
                for (int j = 0; j < SCROLL_COLS; j++) {
                    scroll_buf[i][j] = scroll_buf[i+1][j];
                    scroll_col[i][j] = scroll_col[i+1][j];
                }
            sb_line = SCROLL_LINES - 1;
        }
    }
    scroll_buf[sb_line][sb_col] = c;
    scroll_col[sb_line][sb_col] = color;
    sb_col++;

    if (sb_line + 1 > scroll_total) {
        scroll_total = sb_line + 1;
        if (scroll_total > SCROLL_LINES) scroll_total = SCROLL_LINES;
    }
}

/* ============================================
   Публичные функции вывода
   ============================================ */
void vga_putchar(char c) {
    scroll_buffer_putchar(c);
    if (scroll_offset == 0) scroll_redraw();
}

void vga_print(const char* str) {
    while (*str) vga_putchar(*str++);
}

void vga_print_color(const char* str, unsigned char col) {
    unsigned char old = color;
    color = (VGA_BLACK << 4) | col;
    vga_print(str);
    color = old;
}

void vga_print_dec(int n) {
    char buf[16];
    int i = 0;
    int neg = 0;
    unsigned int u;

    if (n == 0) { vga_putchar('0'); return; }

    if (n < 0) { neg = 1; u = (unsigned int)(-(n + 1)) + 1; }
    else u = (unsigned int)n;

    while (u > 0 && i < 15) {
        buf[i++] = '0' + (u % 10);
        u /= 10;
    }

    if (neg) vga_putchar('-');
    while (i > 0) vga_putchar(buf[--i]);
}

void vga_print_hex32(unsigned int n) {
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        int d = (n >> (28 - i * 4)) & 0xF;
        buf[2 + i] = d < 10 ? '0' + d : 'a' + d - 10;
    }
    buf[10] = 0;
    vga_print(buf);
}

/* ============================================
   Управление
   ============================================ */
void vga_clear(void) {
    for (int i = 0; i < SCROLL_LINES; i++)
        for (int j = 0; j < SCROLL_COLS; j++) {
            scroll_buf[i][j] = ' ';
            scroll_col[i][j] = color;
        }
    scroll_total = 0;
    scroll_offset = 0;
    sb_line = 0;
    sb_col = 0;
    scroll_redraw();
}

void vga_banner(void) {
    vga_print_color("   ____      _ _ _          \n", VGA_LIGHT_CYAN);
    vga_print_color("  / ___|___ | (_) |__  _ __(_)\n", VGA_LIGHT_CYAN);
    vga_print_color(" | |   / _ \\| | | '_ \\| '__| |\n", VGA_LIGHT_CYAN);
    vga_print_color(" | |__| (_) | | | |_) | |  | |\n", VGA_LIGHT_CYAN);
    vga_print_color("  \\____\\___/|_|_|_.__/|_|  |_|\n", VGA_LIGHT_CYAN);
    vga_print("\n");
    vga_print_color("       Colibri OS v0.9\n", VGA_LIGHT_GREEN);
    vga_print("");
    vga_print("Type ");
    vga_print_color("'help'", VGA_YELLOW);
    vga_print(" for commands.h.\n");
    vga_print("Scroll: ");
    vga_print_color("Arrow Up / Down", VGA_YELLOW);
    vga_print("\n");
}

void vga_set_color(unsigned char col) {
    color = (VGA_BLACK << 4) | col;
}

/* ============================================
   Скролл (для стрелок)
   ============================================ */
void vga_scroll_up(void) {
    if (scroll_offset < scroll_total - VIEW_LINES) {
        scroll_offset++;
        scroll_redraw();
    }
}

void vga_scroll_down(void) {
    if (scroll_offset > 0) {
        scroll_offset--;
        scroll_redraw();
    }
}