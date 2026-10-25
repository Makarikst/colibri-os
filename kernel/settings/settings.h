#ifndef SETTINGS_H
#define SETTINGS_H

#include "../framebuffer/framebuffer.h"
#include "../utils/utils.h"
#include "../i18n/i18n.h"

#define SETTINGS_COLORS  8
#define SETTINGS_SWATCH  28
#define SETTINGS_BTN_W   100
#define SETTINGS_BTN_H   26

typedef struct {
    int  open;
    int  win_idx;
    int  win_x, win_y, win_w, win_h;

    int  color_idx;
    int  lang_sel;

    int  btn_en_x, btn_en_y;
    int  btn_ru_x, btn_ru_y;
    int  btn_apply_x, btn_apply_y;
    int  palette_x, palette_y;

    /* Кнопки настройки времени */
    int  btn_h_up_x, btn_h_up_y;
    int  btn_h_dn_x, btn_h_dn_y;
    int  btn_m_up_x, btn_m_up_y;
    int  btn_m_dn_x, btn_m_dn_y;

    rtc_time_t temp_time;

    uint32_t* buf;
    int  buf_w, buf_h;
} SettingsApp;

extern SettingsApp settings_app;
extern uint32_t g_desktop_bg;

void settings_init(void);
void settings_open(int win_idx, int x, int y, int w, int h);
void settings_close(void);
int  settings_is_open(void);

void settings_render(SettingsApp* s);
void settings_render_for_window(int wx, int wy, int ww, int wh);

int  settings_handle_click(int mx, int my, int left_down, int left_prev);
int  settings_handle_key(char key);

extern void (*settings_on_apply)(void);

#endif