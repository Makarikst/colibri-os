#include "menupkm.h"
#include "../framebuffer/font8x16.h"

/* ============================================
   Инициализация / закрытие
   ============================================ */
void menu_init(ContextMenu* m) {
    m->open = 0;
    m->x = 0;
    m->y = 0;
    m->w = MENU_WIDTH;
    m->h = 0;
    m->count = 0;
    m->target_icon = -1;
}

void menu_close(ContextMenu* m) {
    m->open = 0;
    m->count = 0;
    m->target_icon = -1;
}

void menu_open(ContextMenu* m, int x, int y,
               const char* labels[], const int actions[], int count,
               int target_icon)
{
    if (count < 1) { menu_close(m); return; }
    if (count > MENU_MAX_ITEMS) count = MENU_MAX_ITEMS;

    m->open = 1;
    m->x = x;
    m->y = y;
    m->w = MENU_WIDTH;
    m->count = count;
    m->h = count * MENU_ITEM_H;
    m->target_icon = target_icon;

    for (int i = 0; i < count; i++) {
        m->labels[i]  = labels[i];
        m->actions[i] = actions[i];
    }

    /* Если меню выходит за границы экрана — сдвинем */
    if (m->x + m->w > (int)fb_info.width)
        m->x = (int)fb_info.width - m->w;
    if (m->y + m->h > (int)fb_info.height - 48)   /* не залазим на панель */
        m->y = (int)fb_info.height - 48 - m->h;
    if (m->x < 0) m->x = 0;
    if (m->y < 0) m->y = 0;
}

/* ============================================
   Обновление
   ============================================ */
int menu_update(ContextMenu* m, int mx, int my, int left_down, int left_prev) {
    if (!m->open) return MENU_ACTION_NONE;

    int just_pressed = left_down && !left_prev;

    /* Если клик был вне меню — закрыть */
    if (just_pressed) {
        if (mx < m->x || mx >= m->x + m->w ||
            my < m->y || my >= m->y + m->h) {
            menu_close(m);
            return MENU_ACTION_NONE;
        }

        /* Определяем пункт */
        int idx = (my - m->y) / MENU_ITEM_H;
        if (idx >= 0 && idx < m->count) {
            int action = m->actions[idx];
            menu_close(m);
            return action;
        }
    }

    return MENU_ACTION_NONE;
}

/* ============================================
   Отрисовка
   ============================================ */
void menu_draw(ContextMenu* m, uint32_t* buf, int bw, int bh) {
    if (!m->open) return;

    /* Фон */
    for (int y = m->y; y < m->y + m->h; y++) {
        if (y < 0 || y >= bh) continue;
        for (int x = m->x; x < m->x + m->w; x++) {
            if (x < 0 || x >= bw) continue;
            buf[y * bw + x] = MENU_BG;
        }
    }

    /* Рамка */
    for (int x = m->x; x < m->x + m->w; x++) {
        if (x < 0 || x >= bw) continue;
        if (m->y >= 0 && m->y < bh)
            buf[m->y * bw + x] = MENU_BORDER;
        if (m->y + m->h - 1 >= 0 && m->y + m->h - 1 < bh)
            buf[(m->y + m->h - 1) * bw + x] = MENU_BORDER;
    }
    for (int y = m->y; y < m->y + m->h; y++) {
        if (y < 0 || y >= bh) continue;
        if (m->x >= 0 && m->x < bw)
            buf[y * bw + m->x] = MENU_BORDER;
        if (m->x + m->w - 1 >= 0 && m->x + m->w - 1 < bw)
            buf[y * bw + m->x + m->w - 1] = MENU_BORDER;
    }

    /* Разделители между пунктами (тонкие серые полосы) */
    for (int i = 1; i < m->count; i++) {
        int sep_y = m->y + i * MENU_ITEM_H;
        for (int x = m->x + 2; x < m->x + m->w - 2; x++) {
            if (x < 0 || x >= bw) continue;
            if (sep_y < 0 || sep_y >= bh) continue;
            buf[sep_y * bw + x] = MENU_SEPARATOR;
        }
    }

    /* Пункты — рисуем символы через font8x16 */
    for (int i = 0; i < m->count; i++) {
        int iy = m->y + i * MENU_ITEM_H;
        const char* s = m->labels[i];
        int tx = m->x + MENU_TEXT_PAD_X;
        int ty = iy + MENU_TEXT_PAD_Y;

        int cx = tx;
        while (s && *s) {
            unsigned char ch = (unsigned char)*s;
            for (int row = 0; row < 16; row++) {
                unsigned char bits = font8x16[ch][row];
                for (int col = 0; col < 8; col++) {
                    uint32_t color = (bits & (0x80 >> col)) ? MENU_TEXT : MENU_BG;
                    int px = cx + col;
                    int py = ty + row;
                    if (px < 0 || px >= bw) continue;
                    if (py < 0 || py >= bh) continue;
                    buf[py * bw + px] = color;
                }
            }
            cx += 8;
            s++;
        }
    }
}