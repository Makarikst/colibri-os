#include "cx.h"
#include "../vga/vga.h"

void cx_run_code_buf(const char* source, char* out_buf, int max_len) {
    int out_pos = 0;
    const char* p = source;

    while (*p) {
        if (strncmp(p, "label:", 6) == 0) {
            p += 6;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '"') {
                p++;
                const char* prefix = "[APP] ";
                for (int i = 0; prefix[i] && out_pos < max_len - 1; i++) out_buf[out_pos++] = prefix[i];
                while (*p && *p != '"' && out_pos < max_len - 1) out_buf[out_pos++] = *p++;
                if (out_pos < max_len - 1) out_buf[out_pos++] = '\n';
            }
        }
        else if (strncmp(p, "textview(", 9) == 0) {
            p += 9;
            while (*p == ' ' || *p == '\t' || *p == '"') p++;
            const char* prefix = "[UI] ";
            for (int i = 0; prefix[i] && out_pos < max_len - 1; i++) out_buf[out_pos++] = prefix[i];
            while (*p && *p != '"' && *p != ')' && out_pos < max_len - 1) out_buf[out_pos++] = *p++;
            if (out_pos < max_len - 1) out_buf[out_pos++] = '\n';
        }
        p++;
    }
    out_buf[out_pos] = 0;
}

void cx_run_code(const char* source) {
    char buf[1024];
    cx_run_code_buf(source, buf, 1024);
    vga_print(buf);
}

/* ============================================
   Загрузка приложения: MANIFEST + первый screen
   ============================================ */
static const char* cx_skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char* cx_parse_int(const char* p, int* out) {
    int neg = 0, v = 0;
    p = cx_skip_ws(p);
    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
    *out = neg ? -v : v;
    return p;
}

// Копирует значение поля вида  key: "value";  в dst
static void cx_read_field(const char* p, char* dst, int max) {
    int n = 0;
    while (*p == ' ' || *p == '\t' || *p == '"') p++;
    while (*p && *p != '"' && *p != ';' && *p != '\n' && n < max - 1) dst[n++] = *p++;
    dst[n] = 0;
}

int cx_app_load(CxApp* app, const char* src) {
    app->label[0] = 0; app->icon[0] = 0;
    app->has_screen = 0; app->widget_count = 0;

    // --- MANIFEST ---
    for (const char* p = src; *p; p++) {
        if (!app->label[0] && strncmp(p, "label:", 6) == 0) cx_read_field(p + 6, app->label, 32);
        else if (!app->icon[0] && strncmp(p, "icon:", 5) == 0) cx_read_field(p + 5, app->icon, 128);
    }
    if (!app->label[0]) { const char* d = "App"; int i = 0; while (d[i]) { app->label[i] = d[i]; i++; } app->label[i] = 0; }

    // --- первый screen.NAME { ... } ---
    const char* p = src;
    while (*p && strncmp(p, "screen.", 7) != 0) p++;
    if (!*p) return 0;
    while (*p && *p != '{') p++;
    if (!*p) return 0;
    p++;
    app->has_screen = 1;

    int depth = 1, auto_y = 20;
    while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) break; }
        else if (*p == '"') { p++; while (*p && *p != '"') p++; }
        else if (strncmp(p, "textview(", 9) == 0 && app->widget_count < CX_MAX_WIDGETS) {
            CxWidget* w = &app->widgets[app->widget_count];
            w->x = 20; w->y = auto_y; w->text[0] = 0;
            int n = 0;
            p = cx_skip_ws(p + 9);
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && n < CX_TEXT_LEN - 1) w->text[n++] = *p++;
                while (*p && *p != '"') p++;
                if (*p == '"') p++;
            } else {
                while (*p && *p != ')' && n < CX_TEXT_LEN - 1) w->text[n++] = *p++;
            }
            w->text[n] = 0;
            while (*p && *p != ')') p++;
            if (*p == ')') p++;

            p = cx_skip_ws(p);
            if (*p == '[') {
                p++;
                while (*p && *p != ']') {
                    if (strncmp(p, "pos=(", 5) == 0) {
                        p = cx_parse_int(p + 5, &w->x);
                        p = cx_skip_ws(p);
                        if (*p == ',') p++;
                        p = cx_parse_int(p, &w->y);
                        continue;
                    }
                    p++;
                }
                if (*p == ']') p++;
            }
            app->widget_count++;
            auto_y = w->y + 24;
            continue;
        }
        p++;
    }
    return 1;
}