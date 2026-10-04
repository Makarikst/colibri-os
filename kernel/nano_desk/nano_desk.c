#include "nano_desk.h"
#include "../kbd/kbd.h"
#include "../wm/wm.h"


extern const unsigned char font8x16[256][16];

NanoEditor nano;

/* ============================================
   Рисование символа в буфер (с клиппингом по окну)
   ============================================ */
static void nano_draw_char(NanoEditor* n, int x, int y, char c,
                           uint32_t fg, uint32_t bg)
{
    unsigned char ch = (unsigned char)c;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= n->bw) continue;
            if (py < 0 || py >= n->bh) continue;
            if (px < n->win_x || px >= n->win_x + n->win_w) continue;
            if (py < n->win_y || py >= n->win_y + n->win_h) continue;
            n->buf_pixels[py * n->bw + px] = color;
        }
    }
}

static void nano_draw_string(NanoEditor* n, int x, int y, const char* s,
                             uint32_t fg, uint32_t bg)
{
    int cx = x;
    while (*s) {
        nano_draw_char(n, cx, y, *s, fg, bg);
        cx += 8;
        s++;
    }
}

/* ============================================
   Инициализация
   ============================================ */
void nano_desk_init(void) {
    nano.open = 0;
    nano.win_idx = -1;
    nano.cur_x = 0;
    nano.cur_y = 0;
    nano.win_x = nano.win_y = nano.win_w = nano.win_h = 0;
    nano.buf[0] = 0;
    nano.len = 0;
    nano.cursor = 0;
    nano.file_fs_index = -1;
    nano.status[0] = 0;
    nano.status_timer = 0;
    nano.buf_pixels = 0;
    nano.bw = 0;
    nano.bh = 0;
}

void nano_desk_open_empty(int win_idx, int x, int y, int w, int h) {
    nano.open = 1;
    nano.win_idx = win_idx;
    nano.cur_x = x + 6;
    nano.cur_y = y + WM_TITLE_H + 4;
    nano.win_x = x;
    nano.win_y = y;
    nano.win_w = w;
    nano.win_h = h;
    nano.buf[0] = 0;
    nano.len = 0;
    nano.cursor = 0;
    nano.file_fs_index = -1;
    nano.status[0] = 0;
    nano.status_timer = 0;
}

void nano_desk_open_file(int fs_index, int win_idx, int x, int y, int w, int h) {
    nano.open = 1;
    nano.win_idx = win_idx;
    nano.cur_x = x + 6;
    nano.cur_y = y + WM_TITLE_H + 4;
    nano.win_x = x;
    nano.win_y = y;
    nano.win_w = w;
    nano.win_h = h;

    FsObject* o = fs_get(fs_index);
    nano.file_fs_index = fs_index;
    if (o && o->size > 0) {
        int sz = o->size;
        if (sz > NANO_BUF_SIZE - 1) sz = NANO_BUF_SIZE - 1;
        for (int i = 0; i < sz; i++) nano.buf[i] = o->data[i];
        nano.buf[sz] = 0;
        nano.len = sz;
        nano.cursor = sz;
    } else {
        nano.buf[0] = 0;
        nano.len = 0;
        nano.cursor = 0;
    }
    nano.status[0] = 0;
    nano.status_timer = 0;
}

void nano_desk_close(void) {
    nano.open = 0;
    nano.win_idx = -1;
}

/* ============================================
   Сохранение в файл
   ============================================ */
static void nano_save(void) {
    if (nano.file_fs_index < 0) {
        /* Создаём новый файл в ~/Desktop */
        int users = -1, alpha = -1;
        for (int i = 0; i < FS_MAX_OBJECTS; i++) {
            FsObject* o = fs_get(i);
            if (!o || o->type != OBJ_DIR) continue;
            if (o->parent == ROOT_INDEX && strcmp(o->name, "colibri") == 0) {
                /* пропускаем */
            }
        }
        /* Ищем alpha по цепочке */
        int colibri = -1;
        for (int i = 0; i < FS_MAX_OBJECTS; i++) {
            FsObject* o = fs_get(i);
            if (!o || o->type != OBJ_DIR) continue;
            if (o->parent == ROOT_INDEX && strcmp(o->name, "colibri") == 0) colibri = i;
        }
        for (int i = 0; i < FS_MAX_OBJECTS && colibri >= 0; i++) {
            FsObject* o = fs_get(i);
            if (!o || o->type != OBJ_DIR) continue;
            if (o->parent == colibri && strcmp(o->name, "users") == 0) users = i;
        }
        for (int i = 0; i < FS_MAX_OBJECTS && users >= 0; i++) {
            FsObject* o = fs_get(i);
            if (!o || o->type != OBJ_DIR) continue;
            if (o->parent == users && strcmp(o->name, "alpha") == 0) alpha = i;
        }
        if (alpha >= 0) {
            int idx = fs_find_in(alpha, "untitled.txt");
            if (idx < 0) idx = fs_create_in(alpha, "untitled.txt", OBJ_FILE);
            nano.file_fs_index = idx;
        }
    }

    if (nano.file_fs_index >= 0) {
        FsObject* o = fs_get(nano.file_fs_index);
        if (o) {
            int sz = nano.len;
            if (sz > FS_DATA_LEN - 1) sz = FS_DATA_LEN - 1;
            for (int i = 0; i < sz; i++) o->data[i] = nano.buf[i];
            o->data[sz] = 0;
            o->size = sz;
            /* статус */
            const char* msg = "[Saved]";
            int i = 0;
            while (msg[i] && i < 63) { nano.status[i] = msg[i]; i++; }
            nano.status[i] = 0;
            nano.status_timer = 120;
        }
    }
}

/* ============================================
   Обработка клавиш
   ============================================ */
void nano_desk_handle_key(char key) {
    if (!nano.open) return;

    /* === Ctrl+S — сохранить === */
    if (key == KEY_CTRL_S) {
        nano_save();
        return;
    }

    /* Escape — закрыть */
    if (key == KEY_ESC) {
        nano_desk_close();
        return;
    }

    /* ... остальной код без изменений ... */

    if (!nano.open) return;

    /* Escape — закрыть */
    if (key == KEY_ESC) {
        nano_desk_close();
        return;
    }

    /* Enter */
    if (key == '\n') {
        if (nano.len < NANO_BUF_SIZE - 1) {
            for (int i = nano.len; i > nano.cursor; i--)
                nano.buf[i] = nano.buf[i-1];
            nano.buf[nano.cursor] = '\n';
            nano.len++;
            nano.cursor++;
            nano.buf[nano.len] = 0;
        }
        return;
    }

    /* Backspace */
    if (key == '\b') {
        if (nano.cursor > 0) {
            for (int i = nano.cursor - 1; i < nano.len - 1; i++)
                nano.buf[i] = nano.buf[i+1];
            nano.len--;
            nano.cursor--;
            nano.buf[nano.len] = 0;
        }
        return;
    }

    /* Обычный символ */
    if ((unsigned char)key >= 32 && (unsigned char)key < 127) {
        if (nano.len < NANO_BUF_SIZE - 1) {
            for (int i = nano.len; i > nano.cursor; i--)
                nano.buf[i] = nano.buf[i-1];
            nano.buf[nano.cursor] = key;
            nano.len++;
            nano.cursor++;
            nano.buf[nano.len] = 0;
        }
    }
}

/* ============================================
   Отрисовка
   ============================================ */
void nano_desk_render(NanoEditor* n) {
    if (!n->open || !n->buf_pixels) return;

    /* Чёрный фон */
    for (int y = n->win_y + WM_TITLE_H; y < n->win_y + n->win_h; y++) {
        if (y < 0 || y >= n->bh) continue;
        for (int x = n->win_x + WM_BORDER; x < n->win_x + n->win_w - WM_BORDER; x++) {
            if (x < 0 || x >= n->bw) continue;
            n->buf_pixels[y * n->bw + x] = 0x000000;
        }
    }

    int base_x = n->win_x + 8;
    int base_y = n->win_y + WM_TITLE_H + 6;
    int max_cols = (n->win_w - 20) / 8;
    int max_rows = (n->win_h - WM_TITLE_H - 20) / 16;
    if (max_cols < 1) max_cols = 1;
    if (max_rows < 1) max_rows = 1;

    /* Текст */
    if (n->len > 0) {
        int col = 0, row = 0;
        for (int i = 0; i < n->len && row < max_rows; i++) {
            char c = n->buf[i];
            if (c == '\n') { col = 0; row++; }
            else {
                if (col >= max_cols) { col = 0; row++; }
                if (row < max_rows) {
                    int px = base_x + col * 8;
                    int py = base_y + row * 16;
                    nano_draw_char(n, px, py, c, 0xFFFFFF, 0x000000);
                }
                col++;
            }
        }
    } else {
        /* Подсказка, если пусто */
        nano_draw_string(n, base_x, base_y,
                         "Nano: empty. Type, Ctrl+S = save, Esc = close.",
                         0x808080, 0x000000);
    }

    /* Курсор — вертикальная полоска */
    {
        int col = 0, row = 0;
        int upto = n->cursor;
        if (upto > n->len) upto = n->len;
        for (int i = 0; i < upto; i++) {
            if (n->buf[i] == '\n') { col = 0; row++; }
            else {
                if (col >= max_cols) { col = 0; row++; }
                col++;
            }
        }
        if (row < max_rows) {
            int cx = base_x + col * 8;
            int cy = base_y + row * 16;
            for (int py = 0; py < 16; py++) {
                for (int px = 0; px < 2; px++) {
                    int fx = cx + px, fy = cy + py;
                    if (fx < n->win_x || fx >= n->win_x + n->win_w) continue;
                    if (fy < n->win_y + WM_TITLE_H || fy >= n->win_y + n->win_h) continue;
                    if (fx < 0 || fx >= n->bw) continue;
                    if (fy < 0 || fy >= n->bh) continue;
                    n->buf_pixels[fy * n->bw + fx] = 0xFFFFFF;
                }
            }
        }
    }

    /* Статус */
    if (n->status[0] && n->status_timer > 0) {
        nano_draw_string(n, base_x, n->win_y + n->win_h - 20,
                        n->status, 0xFFFF00, 0x000000);
        n->status_timer--;
    }
}

void nano_desk_render_for_window(int wx, int wy, int ww, int wh) {
    if (!nano.open) return;
    nano.win_x = wx;
    nano.win_y = wy;
    nano.win_w = ww;
    nano.win_h = wh;
    nano.cur_x = wx + 6;
    nano.cur_y = wy + WM_TITLE_H + 4;
    nano_desk_render(&nano);
}