#include "filer.h"
#include "../wm/wm.h"
#include "../utils/utils.h"

Filer filer;

/* ============================================
   Клиппинг
   ============================================ */
static int inside_win(Filer* f, int px, int py) {
    if (px < f->win_x) return 0;
    if (px >= f->win_x + f->win_w) return 0;
    if (py < f->win_y) return 0;
    if (py >= f->win_y + f->win_h) return 0;
    return 1;
}

static void put_pixel(Filer* f, int px, int py, uint32_t color) {
    if (px < 0 || px >= f->buf_w) return;
    if (py < 0 || py >= f->buf_h) return;
    if (!inside_win(f, px, py)) return;
    f->buf[py * f->buf_w + px] = color;
}

static void draw_char(Filer* f, int x, int y, char c, uint32_t fg, uint32_t bg) {
    extern const unsigned char font8x16[256][16];
    unsigned char ch = (unsigned char)c;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            put_pixel(f, x + col, y + row, color);
        }
    }
}

static void draw_string(Filer* f, const char* str, int x, int y,
                        uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*str) {
        draw_char(f, cx, y, *str, fg, bg);
        cx += 8;
        str++;
    }
}

/* ============================================
   Инициализация
   ============================================ */
void filer_init(Filer* f) {
    f->open = 0;
    f->win_idx = -1;
    f->current_dir = ROOT_INDEX;
    f->item_count = 0;
    f->selected = -1;
    f->selected_file_index = -1;
    f->rename_mode = 0;
    f->rename_row = -1;
    f->rename_buf[0] = 0;
    f->rename_len = 0;
    f->buf = 0;
    f->win_x = f->win_y = f->win_w = f->win_h = 0;
    f->buf_w = f->buf_h = 0;
}

void filer_open(Filer* f, int win_idx, int x, int y, int w, int h) {
    f->open = 1;
    f->win_idx = win_idx;
    f->cur_x = x + 6;
    f->cur_y = y + WM_TITLE_H + FILER_HEADER_H;
    f->width = w - 12;
    f->height = h - WM_TITLE_H - FILER_HEADER_H - 8;
    f->up_btn_x = x + 8;
    f->up_btn_y = y + WM_TITLE_H + 4;
    f->up_btn_w = 60;
    f->up_btn_h = 20;
    f->win_x = x;
    f->win_y = y;
    f->win_w = w;
    f->win_h = h;
    f->current_dir = fs_get_current_dir();
    f->selected = -1;
    f->selected_file_index = -1;
    f->rename_mode = 0;
    f->rename_row = -1;
    f->rename_buf[0] = 0;
    f->rename_len = 0;
    filer_reload(f);
}

void filer_close(Filer* f) {
    f->open = 0;
    f->win_idx = -1;
    f->rename_mode = 0;
    f->rename_row = -1;
}

void filer_reload(Filer* f) {
    f->item_count = 0;
    f->selected = -1;
    f->rename_mode = 0;
    f->rename_row = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type == OBJ_FREE) continue;
        if (o->parent != f->current_dir) continue;
        if (f->item_count < FILER_MAX_ITEMS) {
            f->item_indices[f->item_count] = i;
            f->item_count++;
        }
    }
}

/* ============================================
   Переименование — старт
   ============================================ */
void filer_start_rename(Filer* f) {
    if (f->selected < 0 || f->selected >= f->item_count) return;
    FsObject* o = fs_get(f->item_indices[f->selected]);
    if (!o) return;

    f->rename_mode = 1;
    f->rename_row = f->selected;

    int i = 0;
    while (o->name[i] && i < FS_NAME_LEN - 1) {
        f->rename_buf[i] = o->name[i];
        i++;
    }
    f->rename_buf[i] = 0;
    f->rename_len = i;
}

/* ============================================
   Переименование — обработка клавиш
   ============================================ */
int filer_handle_key(Filer* f, char key) {
    if (!f->open || !f->rename_mode) return 0;

    /* Enter — подтвердить */
    if (key == '\n') {
        if (f->rename_len > 0 && f->rename_row >= 0 &&
            f->rename_row < f->item_count) {
            int idx = f->item_indices[f->rename_row];
            FsObject* o = fs_get(idx);
            if (o) {
                int exists = fs_find_in(o->parent, f->rename_buf);
                if (exists < 0 || exists == idx) {
                    /* Переписываем имя */
                    int i = 0;
                    while (f->rename_buf[i] && i < FS_NAME_LEN - 1) {
                        o->name[i] = f->rename_buf[i];
                        i++;
                    }
                    o->name[i] = 0;

                    /* ============================================
                       СМЕНА ТИПА ПО РАСШИРЕНИЮ
                       ============================================ */
                    int new_is_capp = ends_with(o->name, ".capp");
                    int new_is_txt  = ends_with(o->name, ".txt") ||
                                      ends_with(o->name, ".nano");
                    int new_is_yrl  = ends_with(o->name, ".yrl");

                    if (new_is_capp) {
                        /* Вернуть исходный тип приложения (если был) */
                        if (o->saved_type != -1)
                            o->type = o->saved_type;
                        else if (o->type != OBJ_DIR &&
                                 o->type != OBJ_FILE)
                            o->type = OBJ_FILE;
                    }
                    else if (new_is_txt || new_is_yrl) {
                        /* Запомнить текущий тип, если это приложение */
                        if (o->type == OBJ_APP_TERMINAL || o->type == OBJ_APP_NANO ||
                            o->type == OBJ_APP_FILER    || o->type == OBJ_APP_TRASH) {
                            o->saved_type = o->type;
                        }
                        o->type = OBJ_FILE;
                    }
                    else {
                        /* Неизвестное расширение → обычный файл */
                        if (o->type == OBJ_APP_TERMINAL || o->type == OBJ_APP_NANO ||
                            o->type == OBJ_APP_FILER    || o->type == OBJ_APP_TRASH) {
                            o->saved_type = o->type;
                        }
                        o->type = OBJ_FILE;
                    }
                }
            }
        }
        f->rename_mode = 0;
        f->rename_row = -1;
        f->rename_buf[0] = 0;
        f->rename_len = 0;
        return 1;
    }

    /* Esc — отменить */
    if (key == KEY_ESC) {
        f->rename_mode = 0;
        f->rename_row = -1;
        f->rename_buf[0] = 0;
        f->rename_len = 0;
        return 1;
    }

    /* Backspace */
    if (key == '\b') {
        if (f->rename_len > 0) {
            f->rename_len--;
            f->rename_buf[f->rename_len] = 0;
        }
        return 1;
    }

    /* Обычный символ */
    if ((unsigned char)key >= 32 && (unsigned char)key < 127) {
        if (f->rename_len < FS_NAME_LEN - 1) {
            f->rename_buf[f->rename_len++] = key;
            f->rename_buf[f->rename_len] = 0;
        }
        return 1;
    }

    return 1;
}

/* ============================================
   Enter — открыть папку / файл
   ============================================ */
int filer_handle_enter(Filer* f) {
    if (!f->open) return 0;
    if (f->rename_mode) return 0;
    if (f->selected < 0 || f->selected >= f->item_count) return 0;

    int idx = f->item_indices[f->selected];
    FsObject* o = fs_get(idx);
    if (!o) return 0;

    if (o->type == OBJ_DIR) {
        f->current_dir = idx;
        filer_reload(f);
        return 1;
    }

    if (o->type == OBJ_FILE) {
        if (ends_with(o->name, ".txt") || ends_with(o->name, ".nano"))
            return 2;
        return 3;
    }

    return 0;
}

/* ============================================
   Клик
   ============================================ */
int filer_handle_click(Filer* f, int mx, int my) {
    if (!f->open) return 0;

    if (f->rename_mode) {
        f->rename_mode = 0;
        f->rename_row = -1;
        f->rename_buf[0] = 0;
        f->rename_len = 0;
        return 1;
    }

    /* Кнопка Up */
    if (mx >= f->up_btn_x && mx < f->up_btn_x + f->up_btn_w &&
        my >= f->up_btn_y && my < f->up_btn_y + f->up_btn_h) {
        if (f->current_dir != ROOT_INDEX) {
            FsObject* cur = fs_get(f->current_dir);
            if (cur && cur->parent >= 0) {
                f->current_dir = cur->parent;
                filer_reload(f);
                return 1;
            }
        }
        return 1;
    }

    if (mx < f->cur_x || mx >= f->cur_x + f->width) return 0;
    if (my < f->cur_y || my >= f->cur_y + f->height) return 0;

    int row = (my - f->cur_y) / FILER_ITEM_H;
    if (row < 0 || row >= f->item_count) return 0;

    f->selected = row;
    return 1;
}

/* ============================================
   Отрисовка
   ============================================ */
void filer_render(Filer* f) {
    if (!f->open || !f->buf) return;

    for (int y = f->up_btn_y; y < f->up_btn_y + f->up_btn_h; y++)
        for (int x = f->up_btn_x; x < f->up_btn_x + f->up_btn_w; x++)
            put_pixel(f, x, y, 0x404060);
    draw_string(f, "Up", f->up_btn_x + 18, f->up_btn_y + 2, 0xFFFFFF, 0x404060);

    {
        char path[128];
        int p = 0;
        int stack[FS_MAX_OBJECTS];
        int top = 0;
        int cur = f->current_dir;
        while (cur != ROOT_INDEX) {
            stack[top++] = cur;
            FsObject* o = fs_get(cur);
            if (!o) break;
            cur = o->parent;
        }
        path[p++] = '/';
        while (top > 0) {
            FsObject* o = fs_get(stack[--top]);
            if (!o) continue;
            const char* n = o->name;
            while (*n && p < 120) path[p++] = *n++;
            if (top > 0 && p < 120) path[p++] = '/';
        }
        path[p] = 0;
        draw_string(f, path, f->up_btn_x + f->up_btn_w + 12,
                    f->up_btn_y + 2, 0xFFFFFF, 0x1A1A2A);
    }

    for (int i = 0; i < f->item_count; i++) {
        int iy = f->cur_y + i * FILER_ITEM_H;
        if (iy + FILER_ITEM_H > f->cur_y + f->height) break;

        FsObject* o = fs_get(f->item_indices[i]);
        if (!o) continue;

        uint32_t bg = (i == f->selected) ? 0x3050C0 : 0x101828;
        uint32_t fg = 0xFFFFFF;

        for (int yy = iy; yy < iy + FILER_ITEM_H; yy++)
            for (int xx = f->cur_x; xx < f->cur_x + f->width; xx++)
                put_pixel(f, xx, yy, bg);

        const char* prefix = (o->type == OBJ_DIR) ? "[D] " : "[F] ";
        draw_string(f, prefix, f->cur_x + 4, iy + 2, fg, bg);

        if (f->rename_mode && i == f->rename_row) {
            char disp[FS_NAME_LEN + 2];
            int k = 0;
            for (int j = 0; j < f->rename_len; j++) disp[k++] = f->rename_buf[j];
            disp[k++] = '_';
            disp[k] = 0;
            draw_string(f, disp, f->cur_x + 4 + 4 * 8, iy + 2,
                        0xFFFF00, bg);
        } else {
            draw_string(f, o->name, f->cur_x + 4 + 4 * 8, iy + 2, fg, bg);
        }

        if (o->type == OBJ_FILE) {
            char sz[32];
            int n = o->size;
            int ti = 0;
            char tmp[16];
            if (n == 0) tmp[ti++] = '0';
            while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
            int sp = 0;
            while (ti > 0) sz[sp++] = tmp[--ti];
            sz[sp++] = ' '; sz[sp++] = 'B'; sz[sp] = 0;

            int tw = sp * 8;
            draw_string(f, sz, f->cur_x + f->width - tw - 6, iy + 2,
                        0xA0A0A0, bg);
        }
    }
}

void filer_render_for_window(int win_x, int win_y, int win_w, int win_h) {
    if (!filer.open) return;
    filer.win_x = win_x;
    filer.win_y = win_y;
    filer.win_w = win_w;
    filer.win_h = win_h;
    filer.cur_x = win_x + 6;
    filer.cur_y = win_y + WM_TITLE_H + FILER_HEADER_H;
    filer.up_btn_x = win_x + 8;
    filer.up_btn_y = win_y + WM_TITLE_H + 4;
    filer_render(&filer);
}