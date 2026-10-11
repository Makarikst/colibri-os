#include "html.h"
#include "html_js.h"
#include "../net/http.h"
#include "../kmalloc/kmalloc.h"

extern const unsigned char font8x16[256][16];

static void h_putchar(HtmlDoc* doc, int x, int y, char c,
                      uint32_t fg, uint32_t bg, int underline)
{
    (void)underline;
    unsigned char ch = (unsigned char)c;
    if (ch >= 128) ch = '?';
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px < doc->win_x || px >= doc->win_x + doc->win_w) continue;
            if (py < doc->win_y || py >= doc->win_y + doc->win_h) continue;
            if (px < 0 || px >= doc->fb_w) continue;
            if (py < 0 || py >= doc->fb_h) continue;
            doc->fb[py * doc->fb_w + px] = color;
        }
    }
}

void html_init(HtmlDoc* doc) {
    doc->cursor_x = 0;
    doc->cursor_y = 0;
    doc->scroll_y = 0;
    doc->content_height = 0;
    doc->color = 0xFFFFFF;
    doc->is_center = 0;
    doc->is_bold = 0;
    doc->link_count = 0;
    doc->input_count = 0;
    doc->form_count = 0;
    doc->active_input = -1;
    doc->input_buffer[0] = 0;
    doc->fb = 0;
    doc->fb_w = doc->fb_h = 0;
    doc->base_url[0] = 0;
}

void html_set_win(HtmlDoc* doc, int wx, int wy, int ww, int wh) {
    doc->win_x = wx;
    doc->win_y = wy;
    doc->win_w = ww;
    doc->win_h = wh;
}

void html_set_fb(HtmlDoc* doc, uint32_t* fb, int w, int h) {
    doc->fb = fb;
    doc->fb_w = w;
    doc->fb_h = h;
}

/* ============================================
   KISS-парсер: пропускаем всё между < и >,
   остальное рисуем посимвольно.
   ============================================ */
void html_render(HtmlDoc* doc, const char* html, const char* base_url) {
    (void)base_url;
    if (!html || !doc->fb) return;

    doc->cursor_x = 0;
    doc->cursor_y = 0;
    doc->color = 0xFFFFFF;
    doc->content_height = 0;

    /* Залить контент чёрным */
    for (int py = doc->win_y; py < doc->win_y + doc->win_h; py++) {
        if (py < 0 || py >= doc->fb_h) continue;
        for (int px = doc->win_x; px < doc->win_x + doc->win_w; px++) {
            if (px < 0 || px >= doc->fb_w) continue;
            doc->fb[py * doc->fb_w + px] = 0x000000;
        }
    }

    const char* p = html;
    int skip_tag = 0;

    while (*p) {
        if (*p == '<') {
            const char* t = p + 1;
            int is_close = 0;
            if (*t == '/') { is_close = 1; t++; }

            /* Пропустить до '>' */
            while (*p && *p != '>') p++;
            if (*p == '>') p++;

            /* Начало пропускаемого блока (style / script) */
            if (!is_close) {
                if (t[0]=='s' && t[1]=='t' && t[2]=='y') { skip_tag = 1; continue; }
                if (t[0]=='s' && t[1]=='c' && t[2]=='r') { skip_tag = 2; continue; }
            } else {
                if (skip_tag == 1 && t[0]=='s' && t[1]=='t') skip_tag = 0;
                if (skip_tag == 2 && t[0]=='s' && t[1]=='c') skip_tag = 0;
            }

            /* Перенос строки для блоковых */
            if ((!is_close && t[0]=='b' && t[1]=='r') ||
                (!is_close && t[0]=='p') ||
                (!is_close && t[0]=='d' && t[1]=='i' && t[2]=='v') ||
                (!is_close && t[0]=='l' && t[1]=='i') ||
                (!is_close && t[0]=='h' && t[1]>='1' && t[1]<='6')) {
                doc->cursor_x = 0;
                doc->cursor_y += 18;
            }
            continue;
        }

        /* Внутри style / script — пропускаем */
        if (skip_tag) { p++; continue; }

        if (*p == '\n' || *p == '\r') {
            doc->cursor_x = 0;
            doc->cursor_y += 18;
            p++;
            continue;
        }
        if (*p == '\t') { doc->cursor_x += 8; p++; continue; }
        if (*p == ' ') {
            doc->cursor_x += 8;
            if (doc->cursor_x + 8 >= doc->win_w - 8) {
                doc->cursor_x = 0;
                doc->cursor_y += 18;
            }
            p++;
            continue;
        }

        char c = *p;
        if (c < 32 || c > 126) c = '?';

        if (doc->win_y + 4 + doc->cursor_y + 16 > doc->win_y + doc->win_h) break;

        int sx = doc->win_x + 4 + doc->cursor_x;
        int sy = doc->win_y + 4 + doc->cursor_y;
        h_putchar(doc, sx, sy, c, doc->color, 0x000000, 0);

        doc->cursor_x += 8;
        if (doc->cursor_x + 8 >= doc->win_w - 8) {
            doc->cursor_x = 0;
            doc->cursor_y += 18;
        }
        p++;
    }

    doc->content_height = doc->cursor_y;
}
int html_hit_link(HtmlDoc* doc, int mx, int my) {
    (void)doc; (void)mx; (void)my; return -1;
}
int html_hit_input(HtmlDoc* doc, int mx, int my) {
    (void)doc; (void)mx; (void)my; return -1;
}
void html_scroll(HtmlDoc* doc, int delta) {
    (void)doc; (void)delta;
}