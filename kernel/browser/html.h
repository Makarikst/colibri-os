#ifndef HTML_H
#define HTML_H

#include "../utils/utils.h"

/* ============================================
   Мини-HTML-парсер + рендер

   Поддерживаемые теги:
     <h1>..<h6>       — заголовки разных размеров
     <p>              — абзац (пустая строка до/после)
     <br>             — перенос строки
     <hr>             — горизонтальная линия
     <b>, <i>         — (пока без отдельного начертания, но парсятся)
     <a href="...">   — ссылка (синий + подчёркивание)
     <center>         — центрирование
     <font color=...> — цвет
     <img src=...>    — картинка (заглушка прямоугольником)
     <input>          — текстовое поле (только визуально)
     <form method=get action=...>...</form>
     <script>...</script> — отдаётся JS-движку
     <title>, <head>, <body> — игнорируются как структурные
     &amp; &lt; &gt; &quot; &nbsp; — HTML-entities
   ============================================ */

#define HTML_LINK_MAX 64
#define HTML_FORM_MAX 8
#define HTML_INPUT_MAX 16

typedef struct {
    int  x, y, w, h;          /* экранные координаты (для клика) */
    char href[256];
    char page[256];           /* URL, на который ведёт (заполняется браузером) */
} html_link_t;

typedef struct {
    int  x, y, w, h;          /* экранные координаты */
    char name[64];
    char value[128];
    int  form_id;
} html_input_t;

typedef struct {
    int  y;                   /* документная координата y */
    int  h;                   /* высота */
} html_anchor_t;              /* для скролла */

typedef struct {
/* Позиция рендера (экран) */
    int  cursor_x, cursor_y;
    int  win_x, win_y, win_w, win_h;

    /* Скролл */
    int  scroll_y;            /* на сколько пикселей сдвинут вверх */
    int  content_height;      /* общая высота отрендеренного документа */

    /* Цвета/начертания */
    uint32_t color;
    int  is_center;
    int  is_bold;

    /* Ссылки, поля, формы */
    html_link_t  links[HTML_LINK_MAX];
    int          link_count;

    html_input_t inputs[HTML_INPUT_MAX];
    int          input_count;

    /* Формы: form_id → action + method */
    char form_action[HTML_FORM_MAX][256];
    char form_method[HTML_FORM_MAX][8];
    int  form_count;

    /* Ввод в активном поле */
    int  active_input;        /* индекс input или -1 */
    char input_buffer[128];

    /* Буфер кадра (framebuffer) */
    uint32_t* fb;
    int  fb_w, fb_h;

    /* Базовый URL страницы (для относительных ссылок) */
    char base_url[256];
} HtmlDoc;

void html_init(HtmlDoc* doc);
void html_set_win(HtmlDoc* doc, int wx, int wy, int ww, int wh);
void html_set_fb(HtmlDoc* doc, uint32_t* fb, int w, int h);
void html_render(HtmlDoc* doc, const char* html, const char* base_url);

/* Найти ссылку по клику. Возвращает индекс или -1. */
int  html_hit_link(HtmlDoc* doc, int mx, int my);

/* Найти input по клику. Возвращает индекс или -1. */
int  html_hit_input(HtmlDoc* doc, int mx, int my);

/* Скролл */
void html_scroll(HtmlDoc* doc, int delta);

#endif