#include "startmenu.h"
#include "../desktop/icons.h"
#include "../fs/fs.h"
#include "../utils/utils.h"
#include "../vga/vga.h"

extern const unsigned char font8x16[256][16];

/* ============================================
   Инициализация
   ============================================ */
void startmenu_init(StartMenu* m) {
    m->open = 0;
    m->x = 0;
    m->y = 0;
    m->w = STARTMENU_WIDTH;
    m->h = 0;
    m->count = 0;
    m->hover = -1;
}

/* ============================================
   Открыть меню
   ============================================ */
void startmenu_open(StartMenu* m, int screen_w, int screen_h) {
    m->count = 0;

    /* Ищем папку Applications: colibri → users → alpha → Applications */
    int colibri = -1, users = -1, alpha = -1, apps = -1;

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
    for (int i = 0; i < FS_MAX_OBJECTS && alpha >= 0; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == alpha && strcmp(o->name, "Applications") == 0) apps = i;
    }

    if (apps >= 0) {
        for (int i = 0; i < FS_MAX_OBJECTS; i++) {
            FsObject* o = fs_get(i);
            if (!o || o->type == OBJ_FREE) continue;
            if (o->parent != apps) continue;
            if (!ends_with(o->name, ".capp")) continue;
            if (m->count >= STARTMENU_MAX_ITEMS) break;
            m->fs_indices[m->count++] = i;
        }
    }

    m->h = m->count * STARTMENU_ITEM_H + 8;
    m->x = 4;
    m->y = screen_h - 48 - m->h;   /* 48 = PANEL_HEIGHT */
    m->open = 1;
    m->hover = -1;
}

void startmenu_close(StartMenu* m) {
    m->open = 0;
    m->count = 0;
    m->hover = -1;
}

/* ============================================
   Обновление (мышь)
   Возвращает:
     0 — ничего не выбрано
     >0 — индекс цели + 1
   ============================================ */
int startmenu_update(StartMenu* m, int mx, int my, int left_down, int left_prev) {
    if (!m->open) return 0;

    int inside = (mx >= m->x && mx < m->x + m->w &&
                  my >= m->y && my < m->y + m->h);

    if (inside) {
        int row = (my - m->y - 4) / STARTMENU_ITEM_H;
        if (row >= 0 && row < m->count) m->hover = row;
        else m->hover = -1;
    } else {
        m->hover = -1;
    }

    int just_pressed = left_down && !left_prev;
    if (!just_pressed) return 0;

    if (inside) {
        int row = (my - m->y - 4) / STARTMENU_ITEM_H;
        if (row >= 0 && row < m->count) {
            int fs_idx = m->fs_indices[row];
            startmenu_close(m);
            return fs_idx + 1;
        }
    }

    /* Клик вне меню — закрыть */
    startmenu_close(m);
    return 0;
}

/* ============================================
   Отрисовка
   ============================================ */
void startmenu_draw(StartMenu* m, uint32_t* buf, int bw, int bh) {
    if (!m->open) return;

    /* Фон */
    for (int y = m->y; y < m->y + m->h; y++) {
        if (y < 0 || y >= bh) continue;
        for (int x = m->x; x < m->x + m->w; x++) {
            if (x < 0 || x >= bw) continue;
            buf[y * bw + x] = 0x202840;
        }
    }

    /* Рамка */
    for (int x = m->x; x < m->x + m->w; x++) {
        if (x < 0 || x >= bw) continue;
        if (m->y >= 0 && m->y < bh) buf[m->y * bw + x] = 0x4A6BC0;
        if (m->y + m->h - 1 >= 0 && m->y + m->h - 1 < bh)
            buf[(m->y + m->h - 1) * bw + x] = 0x4A6BC0;
    }
    for (int y = m->y; y < m->y + m->h; y++) {
        if (y < 0 || y >= bh) continue;
        if (m->x >= 0 && m->x < bw) buf[y * bw + m->x] = 0x4A6BC0;
        if (m->x + m->w - 1 >= 0 && m->x + m->w - 1 < bw)
            buf[y * bw + m->x + m->w - 1] = 0x4A6BC0;
    }

    /* Пункты */
    for (int i = 0; i < m->count; i++) {
        int iy = m->y + 4 + i * STARTMENU_ITEM_H;
        FsObject* o = fs_get(m->fs_indices[i]);
        if (!o) continue;

        uint32_t bg = (i == m->hover) ? 0x3050C0 : 0x202840;
        for (int yy = iy; yy < iy + STARTMENU_ITEM_H - 4; yy++)
            for (int xx = m->x + 2; xx < m->x + m->w - 2; xx++)
                if (yy >= 0 && yy < bh && xx >= 0 && xx < bw)
                    buf[yy * bw + xx] = bg;

        /* Иконка */
        const uint32_t (*img)[32] = 0;
        if (ends_with(o->name, ".capp")) {
            if (starts_with(o->name, "Terminal"))     img = icon_terminal;
            else if (starts_with(o->name, "Filer"))   img = icon_filer;
            else if (starts_with(o->name, "Nano"))    img = icon_nano;
        }

        if (img) {
            int icon_x = m->x + 8;
            int icon_y = iy + 4;
            for (int row = 0; row < 32; row++) {
                for (int col = 0; col < 32; col++) {
                    uint32_t c = img[row][col];
                    if (c == 0) continue;
                    int px = icon_x + col, py = icon_y + row;
                    if (px < 0 || px >= bw) continue;
                    if (py < 0 || py >= bh) continue;
                    buf[py * bw + px] = c;
                }
            }
        }

        /* Имя без .capp */
        char name_buf[FS_NAME_LEN];
        int n = 0;
        while (o->name[n] && o->name[n] != '.' && n < FS_NAME_LEN - 1) {
            name_buf[n] = o->name[n];
            n++;
        }
        name_buf[n] = 0;

        /* Текст */
        int tx = m->x + 50;
        int ty = iy + 16;
        for (int k = 0; name_buf[k]; k++) {
            unsigned char ch = (unsigned char)name_buf[k];
            for (int row = 0; row < 16; row++) {
                unsigned char bits = font8x16[ch][row];
                for (int col = 0; col < 8; col++) {
                    uint32_t color = (bits & (0x80 >> col)) ? 0xFFFFFF : bg;
                    int px = tx + k * 8 + col;
                    int py = ty + row;
                    if (px < 0 || px >= bw) continue;
                    if (py < 0 || py >= bh) continue;
                    buf[py * bw + px] = color;
                }
            }
        }
    }
}