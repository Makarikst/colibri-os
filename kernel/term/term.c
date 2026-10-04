#include "term.h"
#include "../wm/wm.h"
#include "../framebuffer/framebuffer.h"
#include "../shell/shell.h"

extern void (*vga_output_target)(char c);
extern const unsigned char font8x16[256][16];

Terminal term;

static Terminal* active_term = 0;

/* ============================================
   Рисование символа
   ============================================ */
static void term_draw_char(Terminal* t, int x, int y, char c, uint32_t fg, uint32_t bg) {
    unsigned char ch = (unsigned char)c;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= t->buf_w) continue;
            if (py < 0 || py >= t->buf_h) continue;
            if (px < t->win_x || px >= t->win_x + t->win_w) continue;
            if (py < t->win_y || py >= t->win_y + t->win_h) continue;
            t->buf[py * t->buf_w + px] = color;
        }
    }
}

static void term_draw_string(Terminal* t, int x, int y, const char* s,
                             uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*s) {
        term_draw_char(t, cx, y, *s, fg, bg);
        cx += 8;
        s++;
    }
}

/* ============================================
   Добавить строку в scrollback
   ============================================ */
static void sb_push_line(Terminal* t, const char* line) {
    if (t->sb_count >= TERM_SCROLLBACK) {
        for (int i = 0; i < TERM_SCROLLBACK - 1; i++)
            for (int j = 0; j <= TERM_COLS; j++)
                t->scrollback[i][j] = t->scrollback[i+1][j];
        t->sb_count = TERM_SCROLLBACK - 1;
    }
    for (int j = 0; j < TERM_COLS; j++)
        t->scrollback[t->sb_count][j] = line[j];
    t->scrollback[t->sb_count][TERM_COLS] = 0;
    t->sb_count++;
}

static void sb_flush_current_row(Terminal* t, int row) {
    char line[TERM_COLS + 1];
    for (int j = 0; j < TERM_COLS; j++) line[j] = t->text[row][j];
    line[TERM_COLS] = 0;
    sb_push_line(t, line);
}

/* ============================================
   Инициализация
   ============================================ */
void term_init(Terminal* t) {
    t->open = 0;
    t->win_idx = -1;
    for (int i = 0; i < TERM_ROWS; i++) {
        for (int j = 0; j < TERM_COLS; j++) t->text[i][j] = ' ';
        t->text[i][TERM_COLS] = 0;
    }
    t->sb_count = 0;
    t->view_offset = 0;
    t->cursor_col = 0;
    t->cursor_row = 0;
    t->input_len = 0;
    t->input[0] = 0;
    t->win_x = t->win_y = t->win_w = t->win_h = 0;
    t->buf = 0;
    t->buf_w = 0;
    t->buf_h = 0;
}

void term_open(Terminal* t, int win_idx, int x, int y, int w, int h) {
    t->open = 1;
    t->win_idx = win_idx;
    t->cur_x = x + 6;
    t->cur_y = y + WM_TITLE_H + 4;
    t->win_x = x;
    t->win_y = y;
    t->win_w = w;
    t->win_h = h;

    t->cols = (w - 12) / TERM_FONT_W;
    t->rows = (h - WM_TITLE_H - 8) / TERM_FONT_H;
    if (t->cols > TERM_COLS) t->cols = TERM_COLS;
    if (t->rows > TERM_ROWS) t->rows = TERM_ROWS;

    term_clear(t);

    const char* prompt = "colibri:/users/alpha> ";
    while (*prompt) term_putchar(t, *prompt++);
}

void term_close(Terminal* t) {
    if (active_term == t) {
        active_term = 0;
        vga_output_target = 0;
    }
    t->open = 0;
    t->win_idx = -1;
}

void term_clear(Terminal* t) {
    for (int i = 0; i < TERM_ROWS; i++)
        for (int j = 0; j < TERM_COLS; j++) t->text[i][j] = ' ';
    t->cursor_col = 0;
    t->cursor_row = 0;
    t->view_offset = 0;
}

/* ============================================
   Скролл
   ============================================ */
void term_scroll(Terminal* t, int delta) {
    if (!t->open) return;

    t->view_offset += delta;

    int max_off = t->sb_count - t->rows;
    if (max_off < 0) max_off = 0;
    if (t->view_offset > max_off) t->view_offset = max_off;
    if (t->view_offset < 0) t->view_offset = 0;
}

/* ============================================
   Вывод символа
   ============================================ */
void term_putchar(Terminal* t, char c) {
    t->view_offset = 0;

    if (c == '\n') {
        sb_flush_current_row(t, t->cursor_row);
        t->cursor_col = 0;
        t->cursor_row++;
        if (t->cursor_row >= TERM_ROWS) {
            for (int i = 0; i < TERM_ROWS - 1; i++)
                for (int j = 0; j < TERM_COLS; j++)
                    t->text[i][j] = t->text[i+1][j];
            for (int j = 0; j < TERM_COLS; j++)
                t->text[TERM_ROWS-1][j] = ' ';
            t->cursor_row = TERM_ROWS - 1;
        }
        return;
    }
    if (c == '\b') {
        if (t->cursor_col > 0) {
            t->cursor_col--;
            t->text[t->cursor_row][t->cursor_col] = ' ';
        }
        return;
    }
    if (c < 32) return;

    if (t->cursor_col >= TERM_COLS) {
        sb_flush_current_row(t, t->cursor_row);
        t->cursor_col = 0;
        t->cursor_row++;
        if (t->cursor_row >= TERM_ROWS) {
            for (int i = 0; i < TERM_ROWS - 1; i++)
                for (int j = 0; j < TERM_COLS; j++)
                    t->text[i][j] = t->text[i+1][j];
            for (int j = 0; j < TERM_COLS; j++)
                t->text[TERM_ROWS-1][j] = ' ';
            t->cursor_row = TERM_ROWS - 1;
        }
    }
    t->text[t->cursor_row][t->cursor_col] = c;
    t->cursor_col++;
}

static void term_output_hook(char c) {
    if (active_term) term_putchar(active_term, c);
}

void term_activate(Terminal* t) {
    active_term = t;
    vga_output_target = term_output_hook;
}

/* ============================================
   Отрисовка
   ============================================ */
void term_render(Terminal* t) {
    if (!t->open || !t->buf) return;

    /* Чёрный фон */
    for (int row = 0; row < t->rows; row++) {
        for (int col = 0; col < t->cols; col++) {
            int x = t->cur_x + col * TERM_FONT_W;
            int y = t->cur_y + row * TERM_FONT_H;
            for (int py = 0; py < TERM_FONT_H; py++) {
                for (int px = 0; px < TERM_FONT_W; px++) {
                    int fx = x + px, fy = y + py;
                    if (fx < 0 || fx >= t->buf_w) continue;
                    if (fy < 0 || fy >= t->buf_h) continue;
                    if (fx < t->win_x || fx >= t->win_x + t->win_w) continue;
                    if (fy < t->win_y || fy >= t->win_y + t->win_h) continue;
                    t->buf[fy * t->buf_w + fx] = 0x000000;
                }
            }
        }
    }

    if (t->view_offset == 0) {
        for (int row = 0; row < t->rows; row++) {
            for (int col = 0; col < t->cols; col++) {
                char c = t->text[row][col];
                int x = t->cur_x + col * TERM_FONT_W;
                int y = t->cur_y + row * TERM_FONT_H;
                term_draw_char(t, x, y, c, 0xFFFFFF, 0x000000);
            }
        }
    } else {
        int start = t->sb_count - t->view_offset - t->rows;
        for (int row = 0; row < t->rows; row++) {
            int idx = start + row;
            for (int col = 0; col < t->cols; col++) {
                char c = ' ';
                if (idx >= 0 && idx < t->sb_count && col < TERM_COLS)
                    c = t->scrollback[idx][col];
                int x = t->cur_x + col * TERM_FONT_W;
                int y = t->cur_y + row * TERM_FONT_H;
                term_draw_char(t, x, y, c, 0xFFFFFF, 0x000000);
            }
        }
    }

    /* === ОТЛАДКА: показываем sb_count и view_offset === */
    {
        char dbg[32];
        int n = 0;
        dbg[n++] = 's'; dbg[n++] = 'b'; dbg[n++] = '=';
        int v = t->sb_count;
        char tmp[8]; int ti = 0;
        if (v == 0) tmp[ti++] = '0';
        while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; }
        while (ti > 0) dbg[n++] = tmp[--ti];
        dbg[n++] = ' '; dbg[n++] = 'v'; dbg[n++] = '=';
        v = t->view_offset; ti = 0;
        if (v == 0) tmp[ti++] = '0';
        while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; }
        while (ti > 0) dbg[n++] = tmp[--ti];
        dbg[n] = 0;

        term_draw_string(t, t->win_x + 8, t->win_y + WM_TITLE_H + 2,
                         dbg, 0xFFFF00, 0x000000);
    }
}

void term_render_for_window(int win_x, int win_y, int win_w, int win_h) {
    if (!term.open) return;
    term.win_x = win_x;
    term.win_y = win_y;
    term.win_w = win_w;
    term.win_h = win_h;
    term.cur_x = win_x + 6;
    term.cur_y = win_y + WM_TITLE_H + 4;
    term_render(&term);
}

/* ============================================
   Обработка клавиш
   ============================================ */
void term_handle_key(Terminal* t, char key) {
    if (!t->open) return;

    if (key == '\n') {
        term_putchar(t, '\n');
        t->input[t->input_len] = 0;
        term_activate(t);
        shell_execute(t->input);
        vga_output_target = 0;
        active_term = 0;
        const char* prompt = "colibri:/users/alpha> ";
        while (*prompt) term_putchar(t, *prompt++);
        t->input_len = 0;
        t->input[0] = 0;
        return;
    }
    if (key == '\b') {
        if (t->input_len > 0) {
            t->input_len--;
            t->input[t->input_len] = 0;
            term_putchar(t, '\b');
        }
        return;
    }
    if ((unsigned char)key < 32) return;
    if (t->input_len < 255) {
        t->input[t->input_len++] = key;
        term_putchar(t, key);
    }
}