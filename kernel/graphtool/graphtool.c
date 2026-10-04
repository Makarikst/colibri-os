#include "graphtool.h"
#include "../wm/wm.h"
#include "../desktop/desktop.h"

extern const unsigned char font8x16[256][16];

GraphTool graphtool;

/* ============================================
   Палитра 16 цветов
   ============================================ */
static const uint32_t default_palette[GT_COLORS] = {
    0x000000, 0xFFFFFF, 0xC0C0C0, 0x808080,
    0xFF0000, 0xFF8000, 0xFFFF00, 0x80FF00,
    0x00FF00, 0x00FF80, 0x00FFFF, 0x0080FF,
    0x0000FF, 0x8000FF, 0xFF00FF, 0xFF0080
};

/* ============================================
   Рисование символа с клиппингом
   ============================================ */
static void gt_char(GraphTool* g, int x, int y, char c, uint32_t fg, uint32_t bg) {
    unsigned char ch = (unsigned char)c;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px < g->win_x || px >= g->win_x + g->win_w) continue;
            if (py < g->win_y + WM_TITLE_H || py >= g->win_y + g->win_h) continue;
            if (px < 0 || px >= g->buf_w) continue;
            if (py < 0 || py >= g->buf_h) continue;
            g->buf[py * g->buf_w + px] = color;
        }
    }
}

static void gt_string(GraphTool* g, int x, int y, const char* s,
                      uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*s) {
        gt_char(g, cx, y, *s, fg, bg);
        cx += 8;
        s++;
    }
}

/* ============================================
   Инициализация
   ============================================ */
void graphtool_init(void) {
    graphtool.open = 0;
    graphtool.win_idx = -1;
    for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++)
        graphtool.pixels[i] = 0xFFFFFF;
    for (int i = 0; i < GT_COLORS; i++)
        graphtool.palette[i] = default_palette[i];
    graphtool.current_color = 0;
    graphtool.file_fs_index = -1;
    graphtool.mouse_was_down = 0;
    graphtool.buf = 0;
    graphtool.buf_w = graphtool.buf_h = 0;
}

int graphtool_is_open(void) { return graphtool.open; }

/* ============================================
   Пересчёт координат из текущего окна
   ============================================ */
static void gt_recalc_layout(void) {
    Window* w = wm_get(graphtool.win_idx);
    if (!w) return;
    graphtool.win_x = w->x;
    graphtool.win_y = w->y;
    graphtool.win_w = w->w;
    graphtool.win_h = w->h;
    int cw = GT_CANVAS * GT_PIXEL;
    graphtool.canvas_x = w->x + (w->w - cw) / 2;
    graphtool.canvas_y = w->y + WM_TITLE_H + 10;
    graphtool.palette_x = w->x + (w->w - GT_COLORS/2 * GT_SWATCH) / 2;
    graphtool.palette_y = graphtool.canvas_y + cw + 10;
    graphtool.btn_clear_x = w->x + 20;
    graphtool.btn_clear_y = graphtool.palette_y + GT_SWATCH * 2 + 10;
    graphtool.btn_save_x  = w->x + w->w - 20 - GT_BTN_W;
    graphtool.btn_save_y  = graphtool.btn_clear_y;
}

/* ============================================
   Открытие
   ============================================ */
void graphtool_open(int win_idx, int x, int y, int w, int h, int fs_index) {
    graphtool.open = 1;
    graphtool.win_idx = win_idx;
    graphtool.file_fs_index = fs_index;
    graphtool.mouse_was_down = 0;
    gt_recalc_layout();

    if (fs_index >= 0) {
        graphtool_load_from_file(fs_index);
    } else {
        graphtool_clear();
    }
}

void graphtool_close(void) {
    graphtool_save();
    graphtool.open = 0;
    graphtool.win_idx = -1;
}

/* ============================================
   Очистить холст
   ============================================ */
void graphtool_clear(void) {
    for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++)
        graphtool.pixels[i] = 0xFFFFFF;
}

/* ============================================
   Загрузить из файла
   ============================================ */
int graphtool_load_from_file(int fs_index) {
    FsObject* o = fs_get(fs_index);
    if (!o) return 0;

    if (o->size == GT_CANVAS * GT_CANVAS * 4) {
        for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) {
            uint32_t c =
                ((uint32_t)(unsigned char)o->data[i*4    ])       |
                ((uint32_t)(unsigned char)o->data[i*4 + 1] << 8)  |
                ((uint32_t)(unsigned char)o->data[i*4 + 2] << 16) |
                ((uint32_t)(unsigned char)o->data[i*4 + 3] << 24);
            graphtool.pixels[i] = c;
        }
        return 1;
    }
    graphtool_clear();
    return 0;
}

/* ============================================
   Сохранить в файл
   ============================================ */
void graphtool_save(void) {
    if (graphtool.file_fs_index < 0) {
        /* Создаём untitled.png в ~/alpha/Desktop */
        int desktop = desktop_get_folder_index();
        if (desktop < 0) return;

        int new_idx = fs_find_in(desktop, "untitled.png");
        if (new_idx < 0)
            new_idx = fs_create_in(desktop, "untitled.png", OBJ_FILE);

        graphtool.file_fs_index = new_idx;
    }

    FsObject* o = fs_get(graphtool.file_fs_index);
    if (!o) return;

    for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) {
        uint32_t c = graphtool.pixels[i];
        o->data[i*4    ] = (char)( c        & 0xFF);
        o->data[i*4 + 1] = (char)((c >>  8) & 0xFF);
        o->data[i*4 + 2] = (char)((c >> 16) & 0xFF);
        o->data[i*4 + 3] = (char)((c >> 24) & 0xFF);
    }
    o->size = GT_CANVAS * GT_CANVAS * 4;

    desktop_scan_files();
}

/* ============================================
   Обработка клавиш — Ctrl+S
   ============================================ */
int graphtool_handle_key(char key) {
    if (!graphtool.open) return 0;

    /* KEY_CTRL_S = -15 (из kbd.h) */
    if (key == -15) {
        graphtool_save();
        return 1;
    }

    return 0;
}

/* ============================================
   Клик мышью
   ============================================ */
int graphtool_handle_click(int mx, int my, int left_down, int left_prev) {
    if (!graphtool.open) return 0;

    gt_recalc_layout();

    /* Рисование: ЛКМ над холстом */
    if (left_down) {
        int cw = GT_CANVAS * GT_PIXEL;
        if (mx >= graphtool.canvas_x && mx < graphtool.canvas_x + cw &&
            my >= graphtool.canvas_y && my < graphtool.canvas_y + cw) {
            int px = (mx - graphtool.canvas_x) / GT_PIXEL;
            int py = (my - graphtool.canvas_y) / GT_PIXEL;
            if (px >= 0 && px < GT_CANVAS && py >= 0 && py < GT_CANVAS) {
                graphtool.pixels[py * GT_CANVAS + px] =
                    graphtool.palette[graphtool.current_color];
                graphtool.mouse_was_down = left_down;
                return 1;
            }
        }
    }

    int just_pressed = left_down && !left_prev;
    if (!just_pressed) {
        graphtool.mouse_was_down = left_down;
        return 0;
    }

    /* Палитра */
    if (mx >= graphtool.palette_x && my >= graphtool.palette_y) {
        int per_row = GT_COLORS / 2;
        int col = (mx - graphtool.palette_x) / GT_SWATCH;
        int row = (my - graphtool.palette_y) / GT_SWATCH;
        if (col >= 0 && col < per_row && row >= 0 && row < 2) {
            int idx = row * per_row + col;
            if (idx >= 0 && idx < GT_COLORS)
                graphtool.current_color = idx;
            return 1;
        }
    }

    /* Очистить */
    if (mx >= graphtool.btn_clear_x && mx < graphtool.btn_clear_x + GT_BTN_W &&
        my >= graphtool.btn_clear_y && my < graphtool.btn_clear_y + GT_BTN_H) {
        graphtool_clear();
        return 1;
    }

    /* Сохранить */
    if (mx >= graphtool.btn_save_x && mx < graphtool.btn_save_x + GT_BTN_W &&
        my >= graphtool.btn_save_y && my < graphtool.btn_save_y + GT_BTN_H) {
        graphtool_save();
        return 1;
    }

    return 0;
}

/* ============================================
   Отрисовка
   ============================================ */
void graphtool_render(GraphTool* g) {
    if (!g->open || !g->buf) return;

    /* Фон окна */
    for (int y = g->win_y + WM_TITLE_H; y < g->win_y + g->win_h; y++) {
        if (y < 0 || y >= g->buf_h) continue;
        for (int x = g->win_x + WM_BORDER; x < g->win_x + g->win_w - WM_BORDER; x++) {
            if (x < 0 || x >= g->buf_w) continue;
            g->buf[y * g->buf_w + x] = 0x2A2A35;
        }
    }

    /* Холст */
    for (int py = 0; py < GT_CANVAS; py++) {
        for (int px = 0; px < GT_CANVAS; px++) {
            uint32_t c = g->pixels[py * GT_CANVAS + px];
            for (int dy = 0; dy < GT_PIXEL; dy++) {
                for (int dx = 0; dx < GT_PIXEL; dx++) {
                    int fx = g->canvas_x + px * GT_PIXEL + dx;
                    int fy = g->canvas_y + py * GT_PIXEL + dy;
                    if (fx < g->win_x || fx >= g->win_x + g->win_w) continue;
                    if (fy < g->win_y + WM_TITLE_H || fy >= g->win_y + g->win_h) continue;
                    if (fx < 0 || fx >= g->buf_w) continue;
                    if (fy < 0 || fy >= g->buf_h) continue;
                    g->buf[fy * g->buf_w + fx] = c;
                }
            }
        }
    }

    /* Палитра 8×2 */
    int per_row = GT_COLORS / 2;
    for (int i = 0; i < GT_COLORS; i++) {
        int row = i / per_row;
        int col = i % per_row;
        int sx = g->palette_x + col * GT_SWATCH;
        int sy = g->palette_y + row * GT_SWATCH;
        for (int dy = 0; dy < GT_SWATCH - 2; dy++) {
            for (int dx = 0; dx < GT_SWATCH - 2; dx++) {
                int fx = sx + dx;
                int fy = sy + dy;
                if (fx < g->win_x || fx >= g->win_x + g->win_w) continue;
                if (fy < g->win_y + WM_TITLE_H || fy >= g->win_y + g->win_h) continue;
                if (fx < 0 || fx >= g->buf_w) continue;
                if (fy < 0 || fy >= g->buf_h) continue;
                g->buf[fy * g->buf_w + fx] = g->palette[i];
            }
        }
        if (i == g->current_color) {
            for (int t = 0; t < GT_SWATCH - 2; t++) {
                int f1x = sx + t, f1y = sy;
                int f2x = sx + t, f2y = sy + GT_SWATCH - 3;
                int f3x = sx, f3y = sy + t;
                int f4x = sx + GT_SWATCH - 3, f4y = sy + t;
                if (f1x < g->buf_w && f1y < g->buf_h && f1y >= g->win_y + WM_TITLE_H)
                    g->buf[f1y * g->buf_w + f1x] = 0xFFFFFF;
                if (f2x < g->buf_w && f2y < g->buf_h && f2y >= g->win_y + WM_TITLE_H)
                    g->buf[f2y * g->buf_w + f2x] = 0xFFFFFF;
                if (f3y < g->buf_h && f3x < g->buf_w && f3y >= g->win_y + WM_TITLE_H)
                    g->buf[f3y * g->buf_w + f3x] = 0xFFFFFF;
                if (f4y < g->buf_h && f4x < g->buf_w && f4y >= g->win_y + WM_TITLE_H)
                    g->buf[f4y * g->buf_w + f4x] = 0xFFFFFF;
            }
        }
    }

    /* Кнопки */
    for (int y = g->btn_clear_y; y < g->btn_clear_y + GT_BTN_H; y++)
        for (int x = g->btn_clear_x; x < g->btn_clear_x + GT_BTN_W; x++)
            if (y < g->buf_h && x < g->buf_w && y >= g->win_y + WM_TITLE_H)
                g->buf[y * g->buf_w + x] = 0x8B2020;
    gt_string(g, g->btn_clear_x + 6, g->btn_clear_y + 5,
              "Очистить", 0xFFFFFF, 0x8B2020);

    for (int y = g->btn_save_y; y < g->btn_save_y + GT_BTN_H; y++)
        for (int x = g->btn_save_x; x < g->btn_save_x + GT_BTN_W; x++)
            if (y < g->buf_h && x < g->buf_w && y >= g->win_y + WM_TITLE_H)
                g->buf[y * g->buf_w + x] = 0x206B20;
    gt_string(g, g->btn_save_x + 6, g->btn_save_y + 5,
              "Сохранить", 0xFFFFFF, 0x206B20);
}

void graphtool_render_for_window(int wx, int wy, int ww, int wh) {
    if (!graphtool.open) return;
    graphtool.win_x = wx;
    graphtool.win_y = wy;
    graphtool.win_w = ww;
    graphtool.win_h = wh;
    gt_recalc_layout();
    graphtool_render(&graphtool);
}