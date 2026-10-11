#include "browser.h"
#include "html.h"
#include "../wm/wm.h"
#include "../kbd/kbd.h"
#include "../net/http.h"
#include "../kmalloc/kmalloc.h"
#include "../fs/fs.h"

extern const unsigned char font8x16[256][16];

Browser browser;

static void b_char(Browser* b, int x, int y, char c, uint32_t fg, uint32_t bg) {
    unsigned char ch = (unsigned char)c;
    if (ch >= 128) ch = '?';
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= b->buf_w) continue;
            if (py < 0 || py >= b->buf_h) continue;
            if (px < b->win_x || px >= b->win_x + b->win_w) continue;
            if (py < b->win_y || py >= b->win_y + b->win_h) continue;
            b->buf[py * b->buf_w + px] = color;
        }
    }
}

static void b_str(Browser* b, int x, int y, const char* s, uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*s) {
        b_char(b, cx, y, *s, fg, bg);
        cx += 8;
        s++;
    }
}

static void b_fill(Browser* b, int x, int y, int w, int h, uint32_t c) {
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= b->buf_h) continue;
        if (py < b->win_y || py >= b->win_y + b->win_h) continue;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= b->buf_w) continue;
            if (px < b->win_x || px >= b->win_x + b->win_w) continue;
            b->buf[py * b->buf_w + px] = c;
        }
    }
}

void browser_init(void) {
    browser.open = 0;
    browser.win_idx = -1;
    browser.page = 0;
    browser.page_len = 0;
    browser.url[0] = 0;
    browser.url_buf[0] = 0;
    browser.url_len = 0;
    browser.url_focus = 0;
    browser.status[0] = 0;
    browser.status_timer = 0;
    browser.buf = 0;
    browser.buf_w = 0;
    browser.buf_h = 0;
    browser.content_x = 0;
    browser.content_y = 0;
    browser.content_w = 0;
    browser.content_h = 0;
    browser.win_x = 0;
    browser.win_y = 0;
    browser.win_w = 0;
    browser.win_h = 0;
    browser.go_btn_x = 0;
    browser.go_btn_y = 0;
    browser.go_btn_w = 32;
    browser.go_btn_h = 22;
}

void browser_open(int win_idx, int x, int y, int w, int h) {
    browser.open = 1;
    browser.win_idx = win_idx;
    browser.win_x = x;
    browser.win_y = y;
    browser.win_w = w;
    browser.win_h = h;

    browser.content_x = x + 2;
    browser.content_y = y + WM_TITLE_H + 30;
    browser.content_w = w - 4;
    browser.content_h = h - WM_TITLE_H - 32;

    browser.go_btn_w = 32;
    browser.go_btn_h = 22;
    browser.go_btn_x = x + w - 8 - browser.go_btn_w;
    browser.go_btn_y = y + WM_TITLE_H + 4;

    if (browser.url_len == 0) {
        const char* def = "http://";
        int i = 0;
        while (def[i] && i < BROWSER_URL_MAX - 1) {
            browser.url_buf[i] = def[i];
            i++;
        }
        browser.url_buf[i] = 0;
        browser.url_len = i;
    }

    html_init(&browser.doc);
    html_set_win(&browser.doc, browser.content_x,
                 browser.content_y, browser.content_w, browser.content_h);

    if (browser.page == 0) {
        const char* hello =
            "<html><body>"
            "<h1>Colibri Browser</h1>"
            "<p>Enter URL, press Enter or click > to go.</p>"
            "<p>Try: <a href=\"http://example.com/\">example.com</a></p>"
            "</body></html>";
        int n = 0;
        while (hello[n]) n++;
        browser.page = (char*)kmalloc(n + 1);
        if (browser.page) {
            for (int i = 0; i <= n; i++) browser.page[i] = hello[i];
            browser.page_len = n;
            if (browser.buf) {
                html_set_fb(&browser.doc, browser.buf, browser.buf_w, browser.buf_h);
                html_render(&browser.doc, browser.page, "");
            }
        }
    }
}

void browser_close(void) {
    if (browser.page) {
        kfree(browser.page);
        browser.page = 0;
    }
    browser.page_len = 0;
    browser.open = 0;
    browser.win_idx = -1;
    browser.url_focus = 0;
}

void browser_navigate(const char* url) {
    if (!url || !url[0]) return;

    /* ==== Локальный файл: file://filename ==== */
    if (strncmp(url, "file://", 7) == 0) {
        const char* fname = url + 7;
        int idx = parse_path(fname, fs_get_current_dir());
        if (idx < 0 || fs_get(idx)->type != OBJ_FILE) {
            /* Ошибка */
            browser.page_len = 0;
            return;
        }
        FsObject* o = fs_get(idx);
        int n = o->size;
        if (browser.page) kfree(browser.page);
        browser.page = (char*)kmalloc(n + 1);
        if (browser.page) {
            for (int i = 0; i < n; i++) browser.page[i] = o->data[i];
            browser.page[n] = 0;
            browser.page_len = n;
        }
        /* Запомним URL */
        int i = 0;
        while (url[i] && i < BROWSER_URL_MAX - 1) { browser.url[i] = url[i]; i++; }
        browser.url[i] = 0;
        browser.url_len = i;

        /* Рендер */
        if (browser.buf) {
            html_init(&browser.doc);
            html_set_fb(&browser.doc, browser.buf, browser.buf_w, browser.buf_h);
            html_set_win(&browser.doc, browser.content_x, browser.content_y,
                         browser.content_w, browser.content_h);
            html_render(&browser.doc, browser.page, url);
        }
        const char* done = "OK";
        i = 0;
        while (done[i] && i < 127) { browser.status[i] = done[i]; i++; }
        browser.status[i] = 0;
        browser.status_timer = 120;
        return;
    }

    /* ... дальше как было (HTTP) ... */

    int i = 0;
    while (url[i] && i < BROWSER_URL_MAX - 1) { browser.url[i] = url[i]; i++; }
    browser.url[i] = 0;
    browser.url_len = i;

    const char* msg = "Loading...";
    i = 0;
    while (msg[i] && i < 127) { browser.status[i] = msg[i]; i++; }
    browser.status[i] = 0;
    browser.status_timer = 300;

    if (browser.page) {
        kfree(browser.page);
        browser.page = 0;
    }
    browser.page_len = 0;

    uint32_t len = 0;
    char* body = http_get(url, &len);

    if (!body) {
        const char* err_html =
            "<html><body><h1>Error</h1><p>Could not load page.</p></body></html>";
        int n = 0;
        while (err_html[n]) n++;
        body = (char*)kmalloc(n + 1);
        if (body) {
            for (int k = 0; k <= n; k++) body[k] = err_html[k];
            len = n;
        }
    }

    browser.page = body;
    browser.page_len = len;

    if (browser.buf) {
        html_init(&browser.doc);
        html_set_fb(&browser.doc, browser.buf, browser.buf_w, browser.buf_h);
        html_set_win(&browser.doc, browser.content_x, browser.content_y,
                     browser.content_w, browser.content_h);
        html_render(&browser.doc, body, url);
    }

    const char* done = "Done";
    i = 0;
    while (done[i] && i < 127) { browser.status[i] = done[i]; i++; }
    browser.status[i] = 0;
    browser.status_timer = 120;
    /* Отладка в статус-бар: размер и первые символы */
    browser.status[0] = 0;
    int si = 0;
    const char* tag = "len=";
    while (tag[si] && si < 20) { browser.status[si] = tag[si]; si++; }
    /* число */
    int v = browser.page_len;
    char num[12]; int ni = 0;
    if (v == 0) num[ni++] = '0';
    while (v > 0 && ni < 11) { num[ni++] = '0' + (v % 10); v /= 10; }
    while (ni > 0 && si < 40) browser.status[si++] = num[--ni];
    /* пробел */
    if (si < 40) browser.status[si++] = ' ';
    /* первые 20 символов */
    for (int k = 0; k < 20 && k < (int)browser.page_len && si < 60; k++) {
        char c = browser.page ? browser.page[k] : '?';
        if (c < 32 || c > 126) c = '.';
        browser.status[si++] = c;
    }
    browser.status[si] = 0;
    browser.status_timer = 600;
}

int browser_handle_click(int mx, int my, int left_down, int left_prev) {
    if (!browser.open) return 0;
    int just_pressed = left_down && !left_prev;
    if (!just_pressed) return 0;

    if (mx >= browser.go_btn_x && mx < browser.go_btn_x + browser.go_btn_w &&
        my >= browser.go_btn_y && my < browser.go_btn_y + browser.go_btn_h) {
        browser.url_buf[browser.url_len] = 0;
        if (browser.url_len > 0) {
            browser_navigate(browser.url_buf);
        }
        return 1;
    }

    int url_x = browser.win_x + 8;
    int url_y = browser.win_y + WM_TITLE_H + 4;
    int url_w = browser.win_w - 16 - browser.go_btn_w - 4;
    int url_h = 22;
    if (mx >= url_x && mx < url_x + url_w &&
        my >= url_y && my < url_y + url_h) {
        browser.url_focus = 1;
        return 1;
    }

    if (mx >= browser.content_x && mx < browser.content_x + browser.content_w &&
        my >= browser.content_y && my < browser.content_y + browser.content_h) {

        int li = html_hit_link(&browser.doc, mx, my);
        if (li >= 0) {
            const char* target = browser.doc.links[li].page;
            if (target[0]) {
                browser_navigate(target);
                return 1;
            }
        }
        browser.url_focus = 0;
        browser.doc.active_input = -1;
        return 1;
    }

    return 0;
}

int browser_handle_key(char key) {
    if (!browser.open) return 0;

    if (browser.url_focus) {
        if (key == '\n') {
            browser.url_buf[browser.url_len] = 0;
            if (browser.url_len > 0) {
                browser_navigate(browser.url_buf);
            }
            return 1;
        }
        if (key == '\b') {
            if (browser.url_len > 0) {
                browser.url_len--;
                browser.url_buf[browser.url_len] = 0;
            }
            return 1;
        }
        if ((unsigned char)key >= 32 && (unsigned char)key < 127) {
            if (browser.url_len < BROWSER_URL_MAX - 1) {
                browser.url_buf[browser.url_len++] = key;
                browser.url_buf[browser.url_len] = 0;
            }
            return 1;
        }
        return 0;
    }

    if (key == KEY_UP)   { html_scroll(&browser.doc, -1); return 1; }
    if (key == KEY_DOWN) { html_scroll(&browser.doc, +1); return 1; }

    return 0;
}

void browser_render(Browser* b) {
    if (!b->open || !b->buf) return;

    /* Верхняя панель */
    b_fill(b, b->win_x + WM_BORDER, b->win_y + WM_TITLE_H,
           b->win_w - WM_BORDER * 2, 30, 0x101018);
    /* Статус-бар */
    b_fill(b, b->win_x + WM_BORDER, b->win_y + b->win_h - 24,
           b->win_w - WM_BORDER * 2, 22, 0x101018);

    /* Адресная строка */
    int url_x = b->win_x + 8;
    int url_y = b->win_y + WM_TITLE_H + 4;
    int url_w = b->win_w - 16 - b->go_btn_w - 4;
    int url_h = 22;
    b_fill(b, url_x, url_y, url_w, url_h, 0x202028);
    b_fill(b, url_x, url_y, url_w, 1, 0x606060);
    b_fill(b, url_x, url_y + url_h - 1, url_w, 1, 0x606060);
    b_fill(b, url_x, url_y, 1, url_h, 0x606060);
    b_fill(b, url_x + url_w - 1, url_y, 1, url_h, 0x606060);

    const char* show = b->url_focus ? b->url_buf : b->url;
    if (!show[0]) show = "";

    int max_chars = (url_w - 8) / 8;
    int len = 0;
    while (show[len]) len++;
    int start = 0;
    if (len > max_chars) start = len - max_chars;

    int cx = url_x + 4, cy = url_y + 3;
    for (int i = start; i < len; i++) {
        b_char(b, cx, cy, show[i], 0xFFFFFF, 0x202028);
        cx += 8;
    }

    if (b->url_focus) {
        int cur_x = url_x + 4 + (len - start) * 8;
        b_fill(b, cur_x, cy, 2, 16, 0xFFFFFF);
    }

    /* Кнопка Go */
    b_fill(b, b->go_btn_x, b->go_btn_y, b->go_btn_w, b->go_btn_h, 0x3050C0);
    b_str(b, b->go_btn_x + 11, b->go_btn_y + 3, ">", 0xFFFFFF, 0x3050C0);

    /* Статус */
    if (b->status_timer > 0) {
        b_str(b, b->win_x + 8, b->win_y + b->win_h - 18,
              b->status, 0xC0C0C0, 0x101018);
        b->status_timer--;
    }
}

void browser_render_for_window(int wx, int wy, int ww, int wh) {
    if (!browser.open) return;

    browser.win_x = wx;
    browser.win_y = wy;
    browser.win_w = ww;
    browser.win_h = wh;

    browser.content_x = wx + 2;
    browser.content_y = wy + WM_TITLE_H + 30;
    browser.content_w = ww - 4;
    browser.content_h = wh - WM_TITLE_H - 32;

    browser.go_btn_x = wx + ww - 8 - browser.go_btn_w;
    browser.go_btn_y = wy + WM_TITLE_H + 4;

    browser.doc.win_x = browser.content_x;
    browser.doc.win_y = browser.content_y;
    browser.doc.win_w = browser.content_w;
    browser.doc.win_h = browser.content_h;

    if (browser.page && browser.buf) {
        html_init(&browser.doc);
        html_set_fb(&browser.doc, browser.buf, browser.buf_w, browser.buf_h);
        html_set_win(&browser.doc, browser.content_x, browser.content_y,
                     browser.content_w, browser.content_h);
        html_render(&browser.doc, browser.page, browser.url);
    }

    browser_render(&browser);
}