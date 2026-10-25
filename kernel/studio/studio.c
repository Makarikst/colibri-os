#include "studio.h"
#include "../wm/wm.h"
#include "../cx/cx.h"
#include "../kbd/kbd.h"
#include "../vga/vga.h"
#include "../desktop/desktop.h"

extern const unsigned char font8x16[256][16];

StudioApp studio;
static char clipboard[2048] = {0};
static int  cursor_pos = 0;

void studio_init(void) {
    studio.open = 0; studio.win_idx = -1; studio.code_len = 0; studio.code[0] = 0; studio.output[0] = 0; studio.buf = 0;
    const char* default_code =
        "MANIFEST {\n"
        "    label: \"MyApp\";\n"
        "    package: \"com.example.myapp\";\n"
        "    icon: \"/colibri/users/alpha/Desktop/untitled.png\";\n"
        "    perms { }\n"
        "}\n"
        "screen.SCREEN1 {\n"
        "    textview(\"Hello!\") [ pos=(20,20); ]\n"
        "}\n"
        "logic.of SCREEN1 { }\n";
    int i = 0; while (default_code[i]) { studio.code[i] = default_code[i]; i++; }
    studio.code[i] = 0; studio.code_len = i;
    cursor_pos = i;
}

void studio_open(int win_idx, int x, int y, int w, int h) {
    studio.open = 1; studio.win_idx = win_idx; studio.win_x = x; studio.win_y = y; studio.win_w = w; studio.win_h = h;
}

void studio_close(void) {
    if (studio.win_idx >= 0) wm_close(studio.win_idx);
    studio.open = 0; studio.win_idx = -1;
}

int studio_handle_key(char key) {
    if (!studio.open) return 0;
    if (key == KEY_ESC) { studio_close(); return 1; }

    if (key == KEY_CTRL_C) {
        for (int i = 0; i < studio.code_len; i++) clipboard[i] = studio.code[i];
        clipboard[studio.code_len] = 0; return 1;
    }
    if (key == KEY_CTRL_V) {
        int i = 0; while (clipboard[i]) {
            for (int k = studio.code_len; k > cursor_pos; k--) studio.code[k] = studio.code[k-1];
            studio.code[cursor_pos++] = clipboard[i++];
            studio.code_len++;
        }
        studio.code[studio.code_len] = 0; return 1;
    }

    if (key == KEY_LEFT)  { if (cursor_pos > 0) cursor_pos--; return 1; }
    if (key == KEY_RIGHT) { if (cursor_pos < studio.code_len) cursor_pos++; return 1; }

    if (key == KEY_UP || key == KEY_DOWN) {
        int line_start = cursor_pos;
        while (line_start > 0 && studio.code[line_start-1] != '\n') line_start--;

        if (key == KEY_UP && line_start > 0) {
            int prev_line_end = line_start - 1;
            int prev_line_start = prev_line_end;
            while (prev_line_start > 0 && studio.code[prev_line_start-1] != '\n') prev_line_start--;
            int offset = cursor_pos - line_start;
            cursor_pos = prev_line_start + offset;
            if (cursor_pos > prev_line_end) cursor_pos = prev_line_end;
        }
        else if (key == KEY_DOWN && cursor_pos < studio.code_len) {
            int next_line_start = cursor_pos;
            while (next_line_start < studio.code_len && studio.code[next_line_start] != '\n') next_line_start++;
            if (next_line_start < studio.code_len) {
                next_line_start++;
                int offset = cursor_pos - line_start;
                cursor_pos = next_line_start + offset;
                int next_line_end = cursor_pos;
                while (next_line_end < studio.code_len && studio.code[next_line_end] != '\n') next_line_end++;
                if (cursor_pos > next_line_end) cursor_pos = next_line_end;
            }
        }
        return 1;
    }

    if (key == '\b') {
        if (cursor_pos > 0) {
            for (int i = cursor_pos - 1; i < studio.code_len - 1; i++) studio.code[i] = studio.code[i+1];
            studio.code_len--; cursor_pos--;
            studio.code[studio.code_len] = 0;
        }
        return 1;
    }

    if ((unsigned char)key >= 32 && (unsigned char)key < 127) {
        if (studio.code_len < 2040) {
            for (int i = studio.code_len; i > cursor_pos; i--) studio.code[i] = studio.code[i-1];
            studio.code[cursor_pos++] = key;
            studio.code_len++;
            studio.code[studio.code_len] = 0;
        }
        return 1;
    }
    return 0;
}

int studio_handle_click(int mx, int my, int left_down, int left_prev) {
    if (!studio.open) return 0;
    int just_pressed = left_down && !left_prev;
    if (just_pressed) {
        int bx = studio.win_x + studio.win_w - 80;
        int by = studio.win_y + studio.win_h - 30;
        if (mx >= bx && mx < bx + 60 && my >= by && my < by + 24) {
            // 1. Выполняем код
            cx_run_code_buf(studio.code, studio.output, 512);

            // 2. Ищем label (для имени файла)
            char app_name[32] = "MyApp";
            const char* p = studio.code;
            while (*p) {
                if (strncmp(p, "label:", 6) == 0) {
                    p += 6;
                    while (*p == ' ' || *p == '"') p++;
                    int i = 0;
                    while (*p && *p != '"' && *p != ';' && i < 30) {
                        if (*p == ' ') app_name[i++] = '_';
                        else app_name[i++] = *p;
                        p++;
                    }
                    app_name[i] = 0;
                    break;
                }
                p++;
            }

            // 3. Создаем .cai файл
            int desk_idx = desktop_get_folder_index();
            if (desk_idx >= 0) {
                char file_name[64];
                int fi = 0; const char* an = app_name;
                while (*an) file_name[fi++] = *an++;
                file_name[fi++] = '.'; file_name[fi++] = 'c'; file_name[fi++] = 'a'; file_name[fi++] = 'i'; file_name[fi] = 0;

                int cai_idx = fs_find_in(desk_idx, file_name);
                if (cai_idx < 0) cai_idx = fs_create_in(desk_idx, file_name, OBJ_FILE);
                if (cai_idx >= 0) {
                    FsObject* o = fs_get(cai_idx);
                    int i = 0;
                    while (studio.code[i] && i < FS_DATA_LEN - 1) { o->data[i] = studio.code[i]; i++; }
                    o->data[i] = 0; o->size = i; desktop_scan_files();
                }
            }
            return 1;
        }
    }
    return 0;
}

void studio_render_for_window(int wx, int wy, int ww, int wh) {
    if (!studio.open) return;
    studio.win_x = wx; studio.win_y = wy; studio.win_w = ww; studio.win_h = wh;

    for (int y = wy + WM_TITLE_H; y < wy + wh; y++)
        for (int x = wx; x < wx + ww; x++)
            if (x < studio.buf_w && y < studio.buf_h && x >= wx && y >= wy)
                studio.buf[y * studio.buf_w + x] = 0x1E1E1E;

    int cx = wx + 10;
    int cy = wy + WM_TITLE_H + 10;

    for (int i = 0; i <= studio.code_len; i++) {
        if (i == cursor_pos) {
            for (int r = 0; r < 16; r++) {
                if (cx < studio.buf_w && cy+r < studio.buf_h && cx >= wx && cy+r >= wy)
                    studio.buf[(cy+r) * studio.buf_w + cx] = 0xFFFFFF;
            }
        }
        if (i == studio.code_len) break;

        if (studio.code[i] == '\n') { cx = wx + 10; cy += 16; continue; }

        unsigned char ch = studio.code[i]; if (cy + 16 > wy + wh - 40) break;

        for (int row = 0; row < 16; row++) {
            unsigned char bits = font8x16[ch][row];
            for (int col = 0; col < 8; col++) {
                uint32_t color = (bits & (0x80 >> col)) ? 0xCCCCCC : 0x1E1E1E;
                if (cx+col < studio.buf_w && cy+row < studio.buf_h && cx+col >= wx && cy+row >= wy)
                    studio.buf[(cy+row) * studio.buf_w + (cx+col)] = color;
            }
        }
        cx += 8; if (cx > wx + ww - 20) { cx = wx + 10; cy += 16; }
    }

    int bx = wx + ww - 80; int by = wy + wh - 30;
    for (int y = by; y < by + 24; y++)
        for (int x = bx; x < bx + 60; x++)
            if (x < studio.buf_w && y < studio.buf_h && y >= wy) studio.buf[y * studio.buf_w + x] = 0x206B20;
    const char* btxt = "Build";
    for (int i = 0; btxt[i]; i++) {
        unsigned char ch = btxt[i];
        for (int row = 0; row < 16; row++) {
            unsigned char bits = font8x16[ch][row];
            for (int col = 0; col < 8; col++) {
                uint32_t color = (bits & (0x80 >> col)) ? 0xFFFFFF : 0x206B20;
                if (bx+6+i*8+col < studio.buf_w && by+4+row < studio.buf_h)
                    studio.buf[(by+4+row) * studio.buf_w + (bx+6+i*8+col)] = color;
            }
        }
    }

    int out_y = wy + wh - 80;
    for (int i = 0; studio.output[i]; i++) {
        if (i > 50 || studio.output[i] == '\n') break;
        unsigned char ch = studio.output[i];
        for (int row = 0; row < 16; row++) {
            unsigned char bits = font8x16[ch][row];
            for (int col = 0; col < 8; col++) {
                uint32_t color = (bits & (0x80 >> col)) ? 0x00FF00 : 0x1E1E1E;
                if (wx+10+i*8+col < studio.buf_w && out_y+row < studio.buf_h)
                    studio.buf[(out_y+row) * studio.buf_w + (wx+10+i*8+col)] = color;
            }
        }
    }
}