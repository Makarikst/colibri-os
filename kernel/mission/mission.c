#include "mission.h"
#include "../wm/wm.h"
#include "../framebuffer/framebuffer.h"
#include "../kbd/kbd.h"
#include "../desktop/desktop.h"

static int open = 0;

#define MC_COLS       4
#define MC_PAD        20
#define MC_WS_W       220
#define MC_WS_H       140
#define MC_TITLE_H    20

/* Хитбоксы для кликов */
static int mc_box_x[16];
static int mc_box_y[16];
static int mc_box_w[16];
static int mc_box_h[16];
static int mc_box_type[16]; /* 0 = пустой стол, 1 = стол с окнами, 2 = полноэкранное окно, 3 = кнопка + */
static int mc_box_ws[16];   /* номер воркспейса или -1 */
static int mc_box_win[16];  /* индекс окна (если type == 2) */
static int mc_box_count = 0;

void mission_open(void)  { open = 1; }
void mission_close(void) { open = 0; }
int  mission_is_open(void){ return open; }
void mission_toggle(void) { open = !open; }

static void dim(uint32_t* buf, int bw, int bh) {
    for (int i = 0; i < bw * bh; i++) {
        uint32_t c = buf[i];
        uint32_t r = ((c >> 16) & 0xFF) * 3 / 4;
        uint32_t g = ((c >> 8)  & 0xFF) * 3 / 4;
        uint32_t b = ( c        & 0xFF) * 3 / 4;
        buf[i] = (r << 16) | (g << 8) | b;
    }
}

static void fill_rect(uint32_t* buf, int bw, int bh, int x, int y, int w, int h, uint32_t c) {
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= bh) continue;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= bw) continue;
            buf[py * bw + px] = c;
        }
    }
}

extern const unsigned char font8x16[256][16];

static void draw_string(uint32_t* buf, int bw, int bh, int x, int y, const char* s, uint32_t color) {
    while (*s) {
        unsigned char ch = (unsigned char)*s;
        for (int row = 0; row < 16; row++) {
            unsigned char bits = font8x16[ch][row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    int px = x + col, py = y + row;
                    if (px >= 0 && px < bw && py >= 0 && py < bh) buf[py * bw + px] = color;
                }
            }
        }
        x += 8;
        s++;
    }
}

void mission_render(uint32_t* buf, int bw, int bh) {
    if (!open) return;
    dim(buf, bw, bh);

    mc_box_count = 0;
    int cur_col = 0;
    int cur_row = 0;

    /* Узнаем, какое окно сейчас самое активное (верхнее) */
    int active_idx = -1;
    if (wm_zcount > 0) {
        active_idx = wm_zorder[wm_zcount - 1];
    }
    Window* active_win = wm_get(active_idx);
    int is_fullscreen_active = (active_win && active_win->active && active_win->is_fullscreen);

    /* Перебираем ВСЕ индексы воркспейсов по порядку */
    for (int ws = 0; ws < wm_workspace_count; ws++) {
        int x = MC_PAD + cur_col * (MC_WS_W + MC_PAD);
        int y = 60 + cur_row * (MC_WS_H + MC_PAD + 24);

        /* Ищем, есть ли на этом воркспейсе полноэкранное окно */
        Window* full_win = 0;
        for (int i = 0; i < wm_zcount; i++) {
            Window* w = wm_get(wm_zorder[i]);
            if (w && w->active && w->workspace == ws && w->is_fullscreen) {
                full_win = w;
                break;
            }
        }

        if (full_win) {
            /* === РИСУЕМ ПОЛНОЭКРАННОЕ ОКНО (как отдельную карточку) === */
            int is_current = (full_win == active_win);
            uint32_t border = is_current ? 0x2A6AD0 : 0x444444;

            fill_rect(buf, bw, bh, x - 2, y - 2, MC_WS_W + 4, MC_WS_H + 4, border);
            fill_rect(buf, bw, bh, x, y, MC_WS_W, MC_WS_H, 0xFFFFFF);
            fill_rect(buf, bw, bh, x, y, MC_WS_W, MC_TITLE_H, 0x2A6AD0);
            if (full_win->title) draw_string(buf, bw, bh, x + 8, y + 4, full_win->title, 0xFFFFFF);
            if (full_win->title) draw_string(buf, bw, bh, x + 10, y + MC_WS_H + 6, full_win->title, 0xFFFFFF);

            mc_box_x[mc_box_count] = x;
            mc_box_y[mc_box_count] = y;
            mc_box_w[mc_box_count] = MC_WS_W;
            mc_box_h[mc_box_count] = MC_WS_H;
            mc_box_type[mc_box_count] = 2;
            mc_box_ws[mc_box_count] = ws;
            // ищем реальный индекс этого окна
            int real_idx = -1;
            for (int k = 0; k < WM_MAX_WINDOWS; k++) {
                if (wm_get(k) == full_win) { real_idx = k; break; }
            }
            mc_box_win[mc_box_count] = real_idx;
            mc_box_count++;
        } else {
            /* === РИСУЕМ ОБЫЧНЫЙ РАБОЧИЙ СТОЛ === */
            int is_current = (ws == wm_current_workspace) && !is_fullscreen_active;
            uint32_t border = is_current ? 0x2A6AD0 : 0x444444;

            fill_rect(buf, bw, bh, x - 2, y - 2, MC_WS_W + 4, MC_WS_H + 4, border);
            fill_rect(buf, bw, bh, x, y, MC_WS_W, MC_WS_H, DESKTOP_BG);

            /* Рисуем окна внутри стола (не полноэкранные) */
            for (int i = 0; i < wm_zcount; i++) {
                int idx = wm_zorder[i];
                Window* w = wm_get(idx);
                if (!w || !w->active || w->workspace != ws || w->is_fullscreen) continue;

                int sw = MC_WS_W * w->w / 1024;
                int sh = MC_WS_H * w->h / 768;
                if (sw < 16) sw = 16;
                if (sh < 16) sh = 16;

                int sx = MC_WS_W * w->x / 1024;
                int sy = MC_WS_H * w->y / 768;

                if (sx + sw > MC_WS_W) sx = MC_WS_W - sw;
                if (sy + sh > MC_WS_H) sy = MC_WS_H - sh;
                if (sx < 0) sx = 0;
                if (sy < 0) sy = 0;

                fill_rect(buf, bw, bh, x + sx, y + sy, sw, sh, 0xFFFFFF);
                fill_rect(buf, bw, bh, x + sx, y + sy, sw, 8, 0x2A6AD0);
            }

            char label[32];
            label[0] = 'W'; label[1] = 'S'; label[2] = ' ';
            label[3] = '0' + (ws + 1) / 10;
            label[4] = '0' + (ws + 1) % 10;
            label[5] = 0;
            draw_string(buf, bw, bh, x + (MC_WS_W - 40)/2, y + MC_WS_H + 6, label, 0xFFFFFF);

            mc_box_x[mc_box_count] = x;
            mc_box_y[mc_box_count] = y;
            mc_box_w[mc_box_count] = MC_WS_W;
            mc_box_h[mc_box_count] = MC_WS_H;
            mc_box_type[mc_box_count] = 1;
            mc_box_ws[mc_box_count] = ws;
            mc_box_count++;
        }

        cur_col++;
        if (cur_col >= MC_COLS) { cur_col = 0; cur_row++; }
    }

    /* 3. Кнопка + */
    int x = MC_PAD + cur_col * (MC_WS_W + MC_PAD);
    int y = 60 + cur_row * (MC_WS_H + MC_PAD + 24);

    fill_rect(buf, bw, bh, x, y, MC_WS_W, MC_WS_H, 0x333333);
    fill_rect(buf, bw, bh, x + MC_WS_W/2 - 20, y + MC_WS_H/2 - 2, 40, 4, 0xFFFFFF);
    fill_rect(buf, bw, bh, x + MC_WS_W/2 - 2, y + MC_WS_H/2 - 20, 4, 40, 0xFFFFFF);
    draw_string(buf, bw, bh, x + (MC_WS_W - 96)/2, y + MC_WS_H + 6, "New Workspace", 0xFFFFFF);

    mc_box_x[mc_box_count] = x;
    mc_box_y[mc_box_count] = y;
    mc_box_w[mc_box_count] = MC_WS_W;
    mc_box_h[mc_box_count] = MC_WS_H;
    mc_box_type[mc_box_count] = 3;
    mc_box_count++;
}

int mission_handle_click(int mx, int my, int left, int prev) {
    if (!open) return 0;
    int just = left && !prev;
    if (!just) return 1;

    for (int i = 0; i < mc_box_count; i++) {
        if (mx >= mc_box_x[i] && mx < mc_box_x[i] + mc_box_w[i] &&
            my >= mc_box_y[i] && my < mc_box_y[i] + mc_box_h[i]) {

            int type = mc_box_type[i];
            if (type == 1) {
                wm_current_workspace = mc_box_ws[i];
            } else if (type == 2) {
                wm_current_workspace = mc_box_ws[i]; // <-- Вот это можно убрать
                int idx = mc_box_win[i];
            } else if (type == 3) {
                if (wm_workspace_count < 16) {
                    wm_workspace_count++;
                    wm_current_workspace = wm_workspace_count - 1;
                }
            }
            open = 0;
            return 1;
        }
    }

    open = 0;
    return 1;
}

int mission_handle_key(char k) {
    if (!open) return 0;
    if (k == KEY_ESC) { open = 0; return 1; }
    return 1;
}