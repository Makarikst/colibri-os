#include "settings.h"
#include "../wm/wm.h"
#include "../fs/fs.h"
#include "../framebuffer/font8x16.h"

extern const unsigned char font8x16[256][16];

SettingsApp settings_app;
uint32_t g_desktop_bg = 0x2A4A7A;

void (*settings_on_apply)(void) = 0;

static const uint32_t palette[SETTINGS_COLORS] = {
    0x2A4A7A, 0x1A3A2A, 0x3A2A4A, 0x4A2A2A,
    0x2A3A4A, 0x1A1A2A, 0x3A4A5A, 0x2A5A6A
};

static void s_put(SettingsApp* s, int px, int py, uint32_t c) {
    if (px < 0 || py < 0 || px >= s->buf_w || py >= s->buf_h) return;
    if (px < s->win_x || px >= s->win_x + s->win_w) return;
    if (py < s->win_y + WM_TITLE_H || py >= s->win_y + s->win_h) return;
    s->buf[py * s->buf_w + px] = c;
}

static void s_fill(SettingsApp* s, int x, int y, int w, int h, uint32_t c) {
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            s_put(s, px, py, c);
}

static void s_char_utf8(SettingsApp* s, int x, int y, uint32_t cp, uint32_t fg, uint32_t bg) {
    const unsigned char* glyph = font_glyph(cp);
    for (int row = 0; row < 16; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            s_put(s, x + col, y + row, color);
        }
    }
}

static void s_str(SettingsApp* s, int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    const char* p = str;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        s_char_utf8(s, cx, y, cp, fg, bg);
        cx += 8;
    }
}

static void s_str_dec(SettingsApp* s, int x, int y, int val, uint32_t fg, uint32_t bg) {
    char buf[16];
    itoa(val, buf, 10);
    s_str(s, x, y, buf, fg, bg);
}

static void s_str_time(SettingsApp* s, int x, int y, rtc_time_t t, uint32_t fg, uint32_t bg) {
    char buf[16];
    buf[0] = '0' + (t.hour / 10); buf[1] = '0' + (t.hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (t.min / 10);  buf[4] = '0' + (t.min % 10);
    buf[5] = ':';
    buf[6] = '0' + (t.sec / 10);  buf[7] = '0' + (t.sec % 10);
    buf[8] = 0;
    s_str(s, x, y, buf, fg, bg);
}

static void s_recalc(void) {
    SettingsApp* s = &settings_app;
    Window* w = wm_get(s->win_idx);
    if (!w) return;
    s->win_x = w->x; s->win_y = w->y;
    s->win_w = w->w; s->win_h = w->h;

    s->palette_x = w->x + 24;
    s->palette_y = w->y + WM_TITLE_H + 50;

    s->btn_en_x = w->x + 24;
    s->btn_en_y = w->y + WM_TITLE_H + 120;
    s->btn_ru_x = w->x + 24 + SETTINGS_BTN_W + 12;
    s->btn_ru_y = s->btn_en_y;

    s->btn_apply_x = w->x + (w->w - SETTINGS_BTN_W) / 2;
    s->btn_apply_y = w->y + w->h - 50;

    int ty = w->y + WM_TITLE_H + 180;
    s->btn_h_dn_x = w->x + 120; s->btn_h_dn_y = ty;
    s->btn_h_up_x = w->x + 170; s->btn_h_up_y = ty;
    s->btn_m_dn_x = w->x + 220; s->btn_m_dn_y = ty;
    s->btn_m_up_x = w->x + 270; s->btn_m_up_y = ty;
}

void settings_init(void) {
    settings_app.open = 0;
    settings_app.win_idx = -1;
    settings_app.color_idx = 0;
    settings_app.lang_sel = i18n_get_lang();
    settings_app.buf = 0;
    settings_on_apply = 0;
}

int settings_is_open(void) { return settings_app.open; }

void settings_open(int win_idx, int x, int y, int w, int h) {
    settings_app.open = 1;
    settings_app.win_idx = win_idx;
    settings_app.win_x = x;
    settings_app.win_y = y;
    settings_app.win_w = w;
    settings_app.win_h = h;
    settings_app.lang_sel = i18n_get_lang();
    settings_app.temp_time = rtc_get_time();
    settings_app.color_idx = 0;
    for (int i = 0; i < SETTINGS_COLORS; i++)
        if (palette[i] == g_desktop_bg) { settings_app.color_idx = i; break; }
    s_recalc();
}

void settings_close(void) {
    if (settings_app.win_idx >= 0) {
        wm_close(settings_app.win_idx); // <--- ЗАКРЫВАЕМ САМО ОКНО
    }
    settings_app.open = 0;
    settings_app.win_idx = -1;
}

int settings_handle_key(char key) {
    if (!settings_app.open) return 0;
    if (key == 27 || key == -13) {
        settings_close();
        return 1;
    }
    return 0;
}

int settings_handle_click(int mx, int my, int left_down, int left_prev) {
    if (!settings_app.open) return 0;
    int just = left_down && !left_prev;
    if (!just) return 0;

    s_recalc();
    SettingsApp* s = &settings_app;

    for (int i = 0; i < SETTINGS_COLORS; i++) {
        int sx = s->palette_x + i * (SETTINGS_SWATCH + 6);
        int sy = s->palette_y;
        if (mx >= sx && mx < sx + SETTINGS_SWATCH &&
            my >= sy && my < sy + SETTINGS_SWATCH) {
            s->color_idx = i;
            return 1;
        }
    }

    if (mx >= s->btn_en_x && mx < s->btn_en_x + SETTINGS_BTN_W &&
        my >= s->btn_en_y && my < s->btn_en_y + SETTINGS_BTN_H) {
        s->lang_sel = LANG_EN; return 1;
    }
    if (mx >= s->btn_ru_x && mx < s->btn_ru_x + SETTINGS_BTN_W &&
        my >= s->btn_ru_y && my < s->btn_ru_y + SETTINGS_BTN_H) {
        s->lang_sel = LANG_RU; return 1;
    }

    if (mx >= s->btn_h_dn_x && mx < s->btn_h_dn_x + 20 && my >= s->btn_h_dn_y && my < s->btn_h_dn_y + 20) {
        s->temp_time.hour--; if (s->temp_time.hour < 0) s->temp_time.hour = 23; return 1;
    }
    if (mx >= s->btn_h_up_x && mx < s->btn_h_up_x + 20 && my >= s->btn_h_up_y && my < s->btn_h_up_y + 20) {
        s->temp_time.hour++; if (s->temp_time.hour > 23) s->temp_time.hour = 0; return 1;
    }
    if (mx >= s->btn_m_dn_x && mx < s->btn_m_dn_x + 20 && my >= s->btn_m_dn_y && my < s->btn_m_dn_y + 20) {
        s->temp_time.min--; if (s->temp_time.min < 0) s->temp_time.min = 59; return 1;
    }
    if (mx >= s->btn_m_up_x && mx < s->btn_m_up_x + 20 && my >= s->btn_m_up_y && my < s->btn_m_up_y + 20) {
        s->temp_time.min++; if (s->temp_time.min > 59) s->temp_time.min = 0; return 1;
    }

    if (mx >= s->btn_apply_x && mx < s->btn_apply_x + SETTINGS_BTN_W &&
        my >= s->btn_apply_y && my < s->btn_apply_y + SETTINGS_BTN_H) {
        g_desktop_bg = palette[s->color_idx];
        i18n_set_lang(s->lang_sel);
        rtc_set_time(s->temp_time);
        if (settings_on_apply) settings_on_apply();
        return 1;
    }
    return 0;
}

void settings_render(SettingsApp* s) {
    if (!s->open || !s->buf) return;

    s_fill(s, s->win_x + WM_BORDER, s->win_y + WM_TITLE_H,
           s->win_w - WM_BORDER * 2, s->win_h - WM_TITLE_H - WM_BORDER, 0x2A2A35);

    s_str(s, s->win_x + 24, s->win_y + WM_TITLE_H + 20, _("display_bg"), 0xFFFFFF, 0x2A2A35);

    for (int i = 0; i < SETTINGS_COLORS; i++) {
        int sx = s->palette_x + i * (SETTINGS_SWATCH + 6);
        int sy = s->palette_y;
        s_fill(s, sx, sy, SETTINGS_SWATCH, SETTINGS_SWATCH, palette[i]);
        if (i == s->color_idx) {
            for (int t = 0; t < SETTINGS_SWATCH; t++) {
                s_put(s, sx + t, sy, 0xFFFFFF);
                s_put(s, sx + t, sy + SETTINGS_SWATCH - 1, 0xFFFFFF);
                s_put(s, sx, sy + t, 0xFFFFFF);
                s_put(s, sx + SETTINGS_SWATCH - 1, sy + t, 0xFFFFFF);
            }
        }
    }

    s_str(s, s->win_x + 24, s->btn_en_y - 24, _("language"), 0xFFFFFF, 0x2A2A35);

    uint32_t en_bg = (s->lang_sel == LANG_EN) ? 0x3050C0 : 0x404060;
    uint32_t ru_bg = (s->lang_sel == LANG_RU) ? 0x3050C0 : 0x404060;
    s_fill(s, s->btn_en_x, s->btn_en_y, SETTINGS_BTN_W, SETTINGS_BTN_H, en_bg);
    s_str(s, s->btn_en_x + 16, s->btn_en_y + 5, _("lang_en"), 0xFFFFFF, en_bg);

    s_fill(s, s->btn_ru_x, s->btn_ru_y, SETTINGS_BTN_W, SETTINGS_BTN_H, ru_bg);
    s_str(s, s->btn_ru_x + 16, s->btn_ru_y + 5, _("lang_ru"), 0xFFFFFF, ru_bg);

    s_str(s, s->win_x + 24, s->btn_h_dn_y + 5, "Time:", 0xFFFFFF, 0x2A2A35);

    s_fill(s, s->btn_h_dn_x, s->btn_h_dn_y, 20, 20, 0x404060);
    s_str(s, s->btn_h_dn_x + 6, s->btn_h_dn_y + 2, "<", 0xFFFFFF, 0x404060);

    s_str_time(s, s->btn_h_dn_x + 28, s->btn_h_dn_y + 2, s->temp_time, 0xFFFFFF, 0x2A2A35);

    s_fill(s, s->btn_h_up_x, s->btn_h_up_y, 20, 20, 0x404060);
    s_str(s, s->btn_h_up_x + 6, s->btn_h_up_y + 2, ">", 0xFFFFFF, 0x404060);

    s_fill(s, s->btn_m_dn_x, s->btn_m_dn_y, 20, 20, 0x404060);
    s_str(s, s->btn_m_dn_x + 6, s->btn_m_dn_y + 2, "<", 0xFFFFFF, 0x404060);

    s_str_dec(s, s->btn_m_dn_x + 28, s->btn_m_dn_y + 2, s->temp_time.sec, 0xFFFFFF, 0x2A2A35);

    s_fill(s, s->btn_m_up_x, s->btn_m_up_y, 20, 20, 0x404060);
    s_str(s, s->btn_m_up_x + 6, s->btn_m_up_y + 2, ">", 0xFFFFFF, 0x404060);

    s_fill(s, s->btn_apply_x, s->btn_apply_y, SETTINGS_BTN_W, SETTINGS_BTN_H, 0x206B20);
    s_str(s, s->btn_apply_x + 20, s->btn_apply_y + 5, _("apply"), 0xFFFFFF, 0x206B20);

    int info_x = s->win_x + 24;
    int info_y = s->win_y + WM_TITLE_H + 220;

    s_str(s, info_x, info_y, "RAM: 16 MB", 0xCCCCCC, 0x2A2A35);
    info_y += 20;
    s_str(s, info_x, info_y, "Disk: ", 0xCCCCCC, 0x2A2A35);
    int used_fs = 0;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_get(i)->type != OBJ_FREE) used_fs++;
    }
    s_str_dec(s, info_x + 48, info_y, used_fs, 0xCCCCCC, 0x2A2A35);
    s_str(s, info_x + 72, info_y, " / 32", 0xCCCCCC, 0x2A2A35);
}

void settings_render_for_window(int wx, int wy, int ww, int wh) {
    if (!settings_app.open) return;
    settings_app.win_x = wx;
    settings_app.win_y = wy;
    settings_app.win_w = ww;
    settings_app.win_h = wh;
    s_recalc();
    settings_render(&settings_app);
}