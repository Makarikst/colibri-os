#include "wm.h"

Window windows[WM_MAX_WINDOWS];
int    wm_zorder[WM_MAX_WINDOWS];
int    wm_zcount = 0;
int    wm_current_idx = -1;

// Рабочие столы
int    wm_current_workspace = 0;
int    wm_workspace_count = 1;

#define COL_TITLE_ACTIVE    0x2A6AD0
#define COL_TITLE_INACTIVE  0x505060
#define COL_BORDER_ACTIVE   0x000000
#define COL_BORDER_INACTIVE 0x303000
#define COL_CLOSE_RED       0xE03030
#define COL_WINDOW_BG       0xFFFFFF

// ... (дальше идет твой старый код wm.c: inside, raise_window, wm_init, wm_create, wm_close, wm_update, draw_window, wm_draw_all, wm_get) ...
// ВАЖНО: В функции wm_create добавь windows[i].workspace = wm_current_workspace;
// И в wm_draw_all добавь if (windows[idx].workspace != wm_current_workspace) continue;

static int inside(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void raise_window(int idx) {
    int pos = -1;
    for (int i = 0; i < wm_zcount; i++)
        if (wm_zorder[i] == idx) { pos = i; break; }
    if (pos < 0) return;
    for (int i = pos; i < wm_zcount - 1; i++)
        wm_zorder[i] = wm_zorder[i + 1];
    wm_zorder[wm_zcount - 1] = idx;
}

void wm_init(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].active = 0;
        windows[i].type = WM_TYPE_EMPTY;
        windows[i].title = 0;
        windows[i].user_data = 0;
        windows[i].dragging = 0;
        windows[i].workspace = 0;
        windows[i].is_fullscreen = 0;
    }
    wm_zcount = 0;
    wm_current_workspace = 0;
    wm_workspace_count = 1;
}

int wm_create(int x, int y, int w, int h, int type, const char* title) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!windows[i].active) {
            windows[i].active = 1;
            windows[i].x = x;
            windows[i].y = y;
            windows[i].w = w;
            windows[i].h = h;
            windows[i].type = type;
            windows[i].title = title;
            windows[i].dragging = 0;
            windows[i].drag_dx = 0;
            windows[i].drag_dy = 0;
            windows[i].user_data = 0;
            windows[i].workspace = wm_current_workspace;
            windows[i].is_fullscreen = 0;
            wm_zorder[wm_zcount++] = i;
            return i;
        }
    }
    return -1;
}

void wm_close(int idx) {
    if (idx < 0 || idx >= WM_MAX_WINDOWS) return;
    if (!windows[idx].active) return;
    windows[idx].active = 0;
    for (int i = 0; i < wm_zcount; i++) {
        if (wm_zorder[i] == idx) {
            for (int j = i; j < wm_zcount - 1; j++) wm_zorder[j] = wm_zorder[j + 1];
            wm_zcount--;
            break;
        }
    }
}

void wm_update(int mx, int my, int left_down, int left_prev) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].active && windows[i].dragging) {
            if (!left_down) { windows[i].dragging = 0; return; }
            windows[i].x = mx - windows[i].drag_dx;
            windows[i].y = my - windows[i].drag_dy;
            if (windows[i].x < 0) windows[i].x = 0;
            if (windows[i].y < 0) windows[i].y = 0;
            if (windows[i].x + windows[i].w > (int)fb_info.width) windows[i].x = (int)fb_info.width - windows[i].w;
            if (windows[i].y + windows[i].h > (int)fb_info.height - 48) windows[i].y = (int)fb_info.height - 48 - windows[i].h;
            return;
        }
    }

    int just_pressed = left_down && !left_prev;
    if (!just_pressed) return;

    for (int zi = wm_zcount - 1; zi >= 0; zi--) {
        int i = wm_zorder[zi];
        if (!windows[i].active) continue;
        if (windows[i].workspace != wm_current_workspace) continue; // Клики только по окнам текущего стола
        if (!inside(mx, my, windows[i].x, windows[i].y, windows[i].w, windows[i].h)) continue;

        int close_x = windows[i].x + windows[i].w - 20;
        int close_y = windows[i].y + 4;
        if (inside(mx, my, close_x, close_y, 16, 16)) { wm_close(i); return; }

        if (inside(mx, my, windows[i].x, windows[i].y, windows[i].w, WM_TITLE_H)) {
            raise_window(i);
            windows[i].dragging = 1;
            windows[i].drag_dx = mx - windows[i].x;
            windows[i].drag_dy = my - windows[i].y;
            return;
        }
        raise_window(i);
        return;
    }
}

static void draw_window(uint32_t* buf, int bw, int bh, int idx, int active) {
    Window* w = &windows[idx];
    if (!w->active) return;

    uint32_t border = active ? COL_BORDER_ACTIVE : COL_BORDER_INACTIVE;
    for (int y = w->y; y < w->y + w->h; y++) {
        if (y < 0 || y >= bh) continue;
        for (int x = w->x; x < w->x + w->w; x++) {
            if (x < 0 || x >= bw) continue;
            if (x < w->x + WM_BORDER || x >= w->x + w->w - WM_BORDER ||
                y < w->y + WM_BORDER || y >= w->y + w->h - WM_BORDER) {
                buf[y * bw + x] = border;
            }
        }
    }

    uint32_t title_col = active ? COL_TITLE_ACTIVE : COL_TITLE_INACTIVE;
    for (int y = w->y + WM_BORDER; y < w->y + WM_TITLE_H; y++) {
        if (y < 0 || y >= bh) continue;
        for (int x = w->x + WM_BORDER; x < w->x + w->w - WM_BORDER; x++) {
            if (x < 0 || x >= bw) continue;
            buf[y * bw + x] = title_col;
        }
    }

    int cx = w->x + w->w - 20;
    int cy = w->y + 4;
    for (int y = cy; y < cy + 16; y++) {
        if (y < 0 || y >= bh) continue;
        for (int x = cx; x < cx + 16; x++) {
            if (x < 0 || x >= bw) continue;
            buf[y * bw + x] = COL_CLOSE_RED;
        }
    }

    for (int y = w->y + WM_TITLE_H; y < w->y + w->h - WM_BORDER; y++) {
        if (y < 0 || y >= bh) continue;
        for (int x = w->x + WM_BORDER; x < w->x + w->w - WM_BORDER; x++) {
            if (x < 0 || x >= bw) continue;
            buf[y * bw + x] = COL_WINDOW_BG;
        }
    }

    if (w->title) {
        extern const unsigned char font8x16[256][16];
        const char* s = w->title;
        int tx = w->x + 8;
        int ty = w->y + 4;
        while (*s && tx + 8 < w->x + w->w - 24) {
            unsigned char ch = (unsigned char)*s;
            for (int row = 0; row < 16; row++) {
                unsigned char bits = font8x16[ch][row];
                for (int col = 0; col < 8; col++) {
                    uint32_t color = (bits & (0x80 >> col)) ? 0xFFFFFF : title_col;
                    int px = tx + col;
                    int py = ty + row;
                    if (px < 0 || px >= bw) continue;
                    if (py < 0 || py >= bh) continue;
                    buf[py * bw + px] = color;
                }
            }
            tx += 8;
            s++;
        }
    }
}

void wm_draw_all(uint32_t* buf, int bw, int bh,
                 void (*content_cb)(int type, int wx, int wy, int ww, int wh))
{
    for (int zi = 0; zi < wm_zcount; zi++) {
        int idx = wm_zorder[zi];
        if (!windows[idx].active) continue;

        // Рисуем только окна текущего стола
        if (windows[idx].workspace != wm_current_workspace) continue;

        int active = (zi == wm_zcount - 1);
        draw_window(buf, bw, bh, idx, active);

        if (content_cb) {
            wm_current_idx = idx;
            content_cb(windows[idx].type, windows[idx].x, windows[idx].y, windows[idx].w, windows[idx].h);
            wm_current_idx = -1;
        }
    }
}

Window* wm_get(int idx) {
    if (idx < 0 || idx >= WM_MAX_WINDOWS) return 0;
    return &windows[idx];
}