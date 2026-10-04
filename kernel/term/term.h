#ifndef TERM_H
#define TERM_H

#include "../utils/utils.h"

#define TERM_COLS        60
#define TERM_ROWS        16
#define TERM_FONT_W      8
#define TERM_FONT_H      16
#define TERM_SCROLLBACK  200

typedef struct {
    int  open;
    int  win_idx;
    int  cur_x, cur_y;
    int  cols, rows;

    int  win_x, win_y, win_w, win_h;

    char text[TERM_ROWS][TERM_COLS + 1];

    char scrollback[TERM_SCROLLBACK][TERM_COLS + 1];
    int  sb_count;
    int  view_offset;

    int  cursor_col;
    int  cursor_row;

    char input[256];
    int  input_len;

    uint32_t* buf;
    int  buf_w, buf_h;
} Terminal;

extern Terminal term;

void term_init(Terminal* t);
void term_open(Terminal* t, int win_idx, int x, int y, int w, int h);
void term_close(Terminal* t);
void term_putchar(Terminal* t, char c);
void term_clear(Terminal* t);
void term_render(Terminal* t);
void term_handle_key(Terminal* t, char key);

void term_scroll(Terminal* t, int delta);

void term_render_for_window(int win_x, int win_y, int win_w, int win_h);

#endif