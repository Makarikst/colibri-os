#include "html_js.h"
#include "../vga/vga.h"

/* ============================================
   Простой "движок": ищем паттерны document.write("...")
   и console.log("...") и alert("...").
   ============================================ */

static void js_print(const char* s) {
    vga_print_color("[JS] ", VGA_LIGHT_MAGENTA);
    vga_print(s);
    vga_putchar('\n');
}

/* Извлечь строку в кавычках после открывающей скобки */
static int extract_string_arg(const char* p, char* out, int out_max) {
    while (*p && *p != '"' && *p != '\'') {
        if (*p == ')' || *p == ';') return 0;
        p++;
    }
    if (!*p) return 0;
    char quote = *p++;
    int i = 0;
    while (*p && *p != quote && i < out_max - 1) {
        if (*p == '\\' && p[1]) { p++; }   /* \n, \", \\ — просто пропускаем слэш */
        out[i++] = *p++;
    }
    out[i] = 0;
    return 1;
}

void html_js_exec_line(const char* line) {
    const char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '/' || *p == '#') return;

    char buf[256];

    /* document.write(...) */
    if (strncmp(p, "document.write", 14) == 0) {
        const char* q = p + 14;
        while (*q && *q != '(') q++;
        if (*q == '(') q++;
        if (extract_string_arg(q, buf, sizeof(buf))) {
            js_print(buf);
        }
        return;
    }

    /* console.log(...) */
    if (strncmp(p, "console.log", 11) == 0) {
        const char* q = p + 11;
        while (*q && *q != '(') q++;
        if (*q == '(') q++;
        if (extract_string_arg(q, buf, sizeof(buf))) {
            js_print(buf);
        }
        return;
    }

    /* alert(...) */
    if (strncmp(p, "alert", 5) == 0) {
        const char* q = p + 5;
        while (*q && *q != '(') q++;
        if (*q == '(') q++;
        if (extract_string_arg(q, buf, sizeof(buf))) {
            vga_print_color("[ALERT] ", VGA_YELLOW);
            vga_print(buf);
            vga_putchar('\n');
        }
        return;
    }

    /* Всё остальное — просто игнорируем */
}

void html_js_run_scripts(const char* html) {
    const char* p = html;
    while (*p) {
        /* Ищем <script */
        if (*p == '<' && strncmp(p, "<script", 7) == 0) {
            const char* end = p;
            while (*end && *end != '>') end++;
            if (*end == '>') end++;

            const char* close = end;
            while (*close &&
                   !(close[0] == '<' && close[1] == '/' &&
                     close[2] == 's' && close[3] == 'c' &&
                     close[4] == 'r' && close[5] == 'i' &&
                     close[6] == 'p' && close[7] == 't'))
                close++;

            /* Выполняем построчно между end и close */
            const char* q = end;
            char line[256];
            int li = 0;
            while (q < close) {
                char c = *q;
                if (c == ';' || c == '\n' || c == '\r') {
                    line[li] = 0;
                    if (li > 0) html_js_exec_line(line);
                    li = 0;
                } else if (li < 255) {
                    line[li++] = c;
                }
                q++;
            }
            if (li > 0) {
                line[li] = 0;
                html_js_exec_line(line);
            }

            p = close;
            continue;
        }
        p++;
    }
}