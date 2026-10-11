#ifndef BROWSER_H
#define BROWSER_H

#include "../framebuffer/framebuffer.h"
#include "../utils/utils.h"
#include "html.h"

#define BROWSER_URL_MAX 256
#define BROWSER_PAGE_MAX 65536

typedef struct {
    int  open;
    int  win_idx;
    int  win_x, win_y, win_w, win_h;

    /* Адресная строка */
    char url[BROWSER_URL_MAX];
    int  url_len;
    int  url_focus;
    char url_buf[BROWSER_URL_MAX];

    /* Кнопка Go ">" */
    int  go_btn_x, go_btn_y, go_btn_w, go_btn_h;

    /* Текущая страница (heap) */
    char*    page;
    uint32_t page_len;

    /* HTML-документ */
    HtmlDoc doc;

    /* Область контента под адресной строкой */
    int  content_x, content_y, content_w, content_h;

    /* Статус-строка */
    char status[128];
    int  status_timer;

    /* Framebuffer */
    uint32_t* buf;
    int  buf_w, buf_h;
} Browser;

extern Browser browser;

void browser_init(void);
void browser_open(int win_idx, int x, int y, int w, int h);
void browser_close(void);

void browser_navigate(const char* url);

int  browser_handle_click(int mx, int my, int left_down, int left_prev);
int  browser_handle_key(char key);

void browser_render(Browser* b);
void browser_render_for_window(int wx, int wy, int ww, int wh);

#endif