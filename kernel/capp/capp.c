#include "capp.h"
#include "../wm/wm.h"
#include "../fs/fs.h"
#include "../cx/cx.h"
#include "../vga/vga.h"
#include "../i18n/i18n.h"
#include "../framebuffer/font8x16.h"

#define CAPP_MAX 4

typedef struct {
    int   used;
    int   win_idx;
    int   fs_idx;
    CxApp app;
} CappInst;

static CappInst  inst[CAPP_MAX];
static uint32_t* g_buf = 0;
static int       g_bw = 0, g_bh = 0;

void capp_init(void) {
    for (int i = 0; i < CAPP_MAX; i++) { inst[i].used = 0; inst[i].win_idx = -1; inst[i].fs_idx = -1; }
}

void capp_set_buffer(uint32_t* buf, int w, int h) { g_buf = buf; g_bw = w; g_bh = h; }

int capp_open(int fs_idx) {
    FsObject* o = fs_get(fs_idx);
    if (!o || o->type == OBJ_FREE) return 0;

    // уже открыто?
    for (int i = 0; i < CAPP_MAX; i++) {
        if (!inst[i].used || inst[i].fs_idx != fs_idx) continue;
        Window* w = wm_get(inst[i].win_idx);
        if (w && w->active && w->type == WM_TYPE_CAPP) return 1;
    }

    int slot = -1;
    for (int i = 0; i < CAPP_MAX; i++) if (!inst[i].used) { slot = i; break; }
    if (slot < 0) return 0;

    cx_app_load(&inst[slot].app, o->data);

    int win = wm_create(160 + slot * 30, 90 + slot * 30, 440, 320, WM_TYPE_CAPP, inst[slot].app.label);
    if (win < 0) return 0;

    inst[slot].used = 1;
    inst[slot].win_idx = win;
    inst[slot].fs_idx = fs_idx;
    return 1;
}

int capp_gc(void) {
    int changed = 0;
    for (int i = 0; i < CAPP_MAX; i++) {
        if (!inst[i].used) continue;
        Window* w = wm_get(inst[i].win_idx);
        if (!w || !w->active || w->type != WM_TYPE_CAPP) {
            inst[i].used = 0; inst[i].win_idx = -1; inst[i].fs_idx = -1;
            changed = 1;
        }
    }
    return changed;
}

// Текст с обрезкой по прямоугольнику окна
static void draw_text(const char* str, int x, int y, uint32_t color,
                      int cx0, int cy0, int cx1, int cy1) {
    int start_x = x;
    const char* s = str;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (cp == '\n') { x = start_x; y += 16; continue; }
        const unsigned char* glyph = font_glyph(cp);
        for (int row = 0; row < 16; row++) {
            unsigned char bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (!(bits & (0x80 >> col))) continue;
                int px = x + col, py = y + row;
                if (px < cx0 || px >= cx1 || py < cy0 || py >= cy1) continue;
                if (px < 0 || px >= g_bw || py < 0 || py >= g_bh) continue;
                g_buf[py * g_bw + px] = color;
            }
        }
        x += 8;
    }
}

void capp_render_for_window(int wx, int wy, int ww, int wh) {
    if (!g_buf) return;
    for (int i = 0; i < CAPP_MAX; i++) {
        if (!inst[i].used) continue;
        if (inst[i].win_idx != wm_current_idx) continue;
        Window* w = wm_get(inst[i].win_idx);
        if (!w || !w->active || w->type != WM_TYPE_CAPP) continue;

        int ox = wx + WM_BORDER, oy = wy + WM_TITLE_H;
        int x1 = wx + ww - WM_BORDER, y1 = wy + wh - WM_BORDER;
        CxApp* app = &inst[i].app;

        if (!app->has_screen) {
            draw_text("No screen defined", ox + 20, oy + 20, 0xC03030, ox, oy, x1, y1);
            return;
        }
        for (int k = 0; k < app->widget_count; k++)
            draw_text(app->widgets[k].text, ox + app->widgets[k].x, oy + app->widgets[k].y,
                      0x000000, ox, oy, x1, y1);
        return;
    }
}