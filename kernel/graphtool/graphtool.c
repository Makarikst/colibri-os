#include "graphtool.h"
#include "../wm/wm.h"
#include "../desktop/desktop.h"
#include "../kbd/kbd.h"

extern const unsigned char font8x16[256][16];

GraphTool graphtool;

static const uint32_t default_palette[GT_COLORS] = {
    0x000000, 0xFFFFFF, 0xC0C0C0, 0x808080,
    0xFF0000, 0xFF8000, 0xFFFF00, 0x80FF00,
    0x00FF00, 0x00FF80, 0x00FFFF, 0x0080FF,
    0x0000FF, 0x8000FF, 0xFF00FF, 0xFF0080
};

// --- Вспомогательные функции ---
static void set_pixel(uint32_t* arr, int x, int y, uint32_t color, int size) {
    int r = size / 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < GT_CANVAS && py >= 0 && py < GT_CANVAS) {
                arr[py * GT_CANVAS + px] = color;
            }
        }
    }
}

static void draw_line(uint32_t* arr, int x0, int y0, int x1, int y1, uint32_t color, int size) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        set_pixel(arr, x0, y0, color, size);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static void draw_rect(uint32_t* arr, int x0, int y0, int x1, int y1, uint32_t color, int size) {
    int sx = x0 < x1 ? x0 : x1, ex = x0 > x1 ? x0 : x1;
    int sy = y0 < y1 ? y0 : y1, ey = y0 > y1 ? y0 : y1;
    draw_line(arr, sx, sy, ex, sy, color, size);
    draw_line(arr, sx, ey, ex, ey, color, size);
    draw_line(arr, sx, sy, sx, ey, color, size);
    draw_line(arr, ex, sy, ex, ey, color, size);
}

static void draw_oval(uint32_t* arr, int cx, int cy, int rx, int ry, uint32_t color, int size) {
    int x = 0, y = ry;
    int rx2 = rx * rx, ry2 = ry * ry;
    int fx2 = 4 * rx2, fy2 = 4 * ry2;
    int p = ry2 - rx2 * ry + rx2 / 4;
    while (x <= y) {
        set_pixel(arr, cx + x, cy + y, color, size); set_pixel(arr, cx - x, cy + y, color, size);
        set_pixel(arr, cx + x, cy - y, color, size); set_pixel(arr, cx - x, cy - y, color, size);
        set_pixel(arr, cx + y, cy + x, color, size); set_pixel(arr, cx - y, cy + x, color, size);
        set_pixel(arr, cx + y, cy - x, color, size); set_pixel(arr, cx - y, cy - x, color, size);
        x++;
        if (p < 0) p += fy2 * x + ry2;
        else { y--; p += fy2 * x - fx2 * y + ry2; }
    }
}

static void draw_triangle(uint32_t* arr, int x0, int y0, int x1, int y1, uint32_t color, int size) {
    int mid_x = (x0 + x1) / 2;
    draw_line(arr, x0, y0, mid_x, y1, color, size);
    draw_line(arr, mid_x, y1, x1, y0, color, size);
    draw_line(arr, x0, y0, x1, y0, color, size);
}

static void draw_hexagon(uint32_t* arr, int cx, int cy, int r, uint32_t color, int size) {
    int dx = r, dy = r / 2;
    draw_line(arr, cx - dx, cy, cx - dx/2, cy - dy, color, size);
    draw_line(arr, cx - dx/2, cy - dy, cx + dx/2, cy - dy, color, size);
    draw_line(arr, cx + dx/2, cy - dy, cx + dx, cy, color, size);
    draw_line(arr, cx + dx, cy, cx + dx/2, cy + dy, color, size);
    draw_line(arr, cx + dx/2, cy + dy, cx - dx/2, cy + dy, color, size);
    draw_line(arr, cx - dx/2, cy + dy, cx - dx, cy, color, size);
}

// --- АЛГОРИТМ ЗАЛИВКИ (Flood Fill) ---
static void flood_fill(uint32_t* arr, int x, int y, uint32_t new_color) {
    uint32_t old_color = arr[y * GT_CANVAS + x];
    if (old_color == new_color) return; // Уже такого цвета

    // Простой стек для координат (чтобы не использовать рекурсию и не переполнить стек)
    static int stack_x[GT_CANVAS * GT_CANVAS];
    static int stack_y[GT_CANVAS * GT_CANVAS];
    int top = 0;

    stack_x[top] = x;
    stack_y[top] = y;
    top++;

    while (top > 0) {
        top--;
        int cx = stack_x[top];
        int cy = stack_y[top];

        if (cx < 0 || cx >= GT_CANVAS || cy < 0 || cy >= GT_CANVAS) continue;
        if (arr[cy * GT_CANVAS + cx] != old_color) continue;

        arr[cy * GT_CANVAS + cx] = new_color;

        // Добавляем соседей (4 направления)
        if (cx > 0) { stack_x[top] = cx - 1; stack_y[top] = cy; top++; }
        if (cx < GT_CANVAS - 1) { stack_x[top] = cx + 1; stack_y[top] = cy; top++; }
        if (cy > 0) { stack_x[top] = cx; stack_y[top] = cy - 1; top++; }
        if (cy < GT_CANVAS - 1) { stack_x[top] = cx; stack_y[top] = cy + 1; top++; }
    }
}

// --- Инициализация ---
void graphtool_init(void) {
    graphtool.open = 0;
    graphtool.win_idx = -1;
    for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) graphtool.pixels[i] = 0xFFFFFF;
    for (int i = 0; i < GT_COLORS; i++) graphtool.palette[i] = default_palette[i];
    graphtool.current_color = 0;
    graphtool.file_fs_index = -1;
    graphtool.mouse_was_down = 0;
    graphtool.save_status = 0;
    graphtool.current_tool = TOOL_BRUSH;
    graphtool.brush_size = 1;
    graphtool.buf = 0;
}

int graphtool_is_open(void) { return graphtool.open; }

static void gt_recalc_layout(void) {
    Window* w = wm_get(graphtool.win_idx);
    if (!w) return;
    graphtool.win_x = w->x; graphtool.win_y = w->y;
    graphtool.win_w = w->w; graphtool.win_h = w->h;
    int cw = GT_CANVAS * GT_PIXEL;

    graphtool.canvas_x = w->x + (w->w - cw) / 2;
    graphtool.canvas_y = w->y + WM_TITLE_H + 40;

    // Панель инструментов (теперь 7 кнопок)
    for (int i = 0; i < 7; i++) {
        graphtool.tool_btn_x[i] = w->x + 10 + i * 40; // Сузили шаг, чтобы влезло
    }
    graphtool.tool_btn_y = w->y + WM_TITLE_H + 5;

    // Панель толщины
    for (int i = 0; i < 5; i++) {
        graphtool.thick_btn_x[i] = w->x + 300 + i * 18;
    }
    graphtool.thick_btn_y = w->y + WM_TITLE_H + 5;

    // Палитра
    graphtool.palette_x = w->x + (w->w - (GT_COLORS/2)*GT_SWATCH) / 2;
    graphtool.palette_y = w->y + w->h - GT_PALETTE_H + 5;

    graphtool.btn_clear_x = w->x + 20;
    graphtool.btn_clear_y = w->y + w->h - 30;
    graphtool.btn_save_x  = w->x + w->w - 100;
    graphtool.btn_save_y  = w->y + w->h - 30;
}

void graphtool_open(int win_idx, int x, int y, int w, int h, int fs_index) {
    graphtool.open = 1;
    graphtool.win_idx = win_idx;
    graphtool.file_fs_index = fs_index;
    graphtool.mouse_was_down = 0;
    graphtool.save_status = 0;
    graphtool.current_tool = TOOL_BRUSH;
    graphtool.brush_size = 1;
    gt_recalc_layout();
    if (fs_index >= 0) graphtool_load_from_file(fs_index);
    else graphtool_clear();
}

void graphtool_close(void) {
    graphtool_save();
    graphtool.open = 0;
    graphtool.win_idx = -1;
}

void graphtool_clear(void) {
    for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) graphtool.pixels[i] = 0xFFFFFF;
}

int graphtool_load_from_file(int fs_index) {
    FsObject* o = fs_get(fs_index);
    if (!o || o->size != GT_CANVAS * GT_CANVAS * 4) { graphtool_clear(); return 0; }
    for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) {
        graphtool.pixels[i] = ((uint32_t)(unsigned char)o->data[i*4]) |
                              ((uint32_t)(unsigned char)o->data[i*4+1] << 8) |
                              ((uint32_t)(unsigned char)o->data[i*4+2] << 16) |
                              ((uint32_t)(unsigned char)o->data[i*4+3] << 24);
    }
    return 1;
}

void graphtool_save(void) {
    int desktop = desktop_get_folder_index();
    if (desktop < 0) { graphtool.save_status = -2; return; }

    /* Если это НОВЫЙ файл, или если мы не смогли найти старый, ищем свободное имя */
    if (graphtool.file_fs_index < 0) {
        char name[FS_NAME_LEN];
        int new_idx = -1;
        for (int n = 0; n < 1000 && new_idx < 0; n++) {
            if (n == 0) {
                strcpy(name, "untitled.png");
            } else {
                char num[8]; int k = 0, v = n, tmp[8];
                while (v > 0) { tmp[k++] = (char)('0' + v % 10); v /= 10; }
                for (int i = 0; i < k; i++) num[i] = tmp[k - 1 - i];
                num[k] = 0;

                int p = 0;
                const char* base = "untitled_";
                while (*base) name[p++] = *base++;
                for (int i = 0; i < k; i++) name[p++] = num[i];
                name[p++] = '.'; name[p++] = 'p'; name[p++] = 'n'; name[p++] = 'g'; name[p] = 0;
            }
            if (fs_find_in(desktop, name) < 0) {
                new_idx = fs_create_in(desktop, name, OBJ_FILE);
            }
        }
        if (new_idx < 0) { graphtool.save_status = -1; return; } // нет места
        graphtool.file_fs_index = new_idx;
    }

    FsObject* o = fs_get(graphtool.file_fs_index);
    if (!o || o->type == OBJ_FREE) {
        graphtool.file_fs_index = -1;
        graphtool.save_status = -1;
        return;
    }

    /* Запись пикселей в файл */
    for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) {
        uint32_t c = graphtool.pixels[i];
        o->data[i*4]   = (char)(c & 0xFF);
        o->data[i*4+1] = (char)((c >> 8) & 0xFF);
        o->data[i*4+2] = (char)((c >> 16) & 0xFF);
        o->data[i*4+3] = (char)((c >> 24) & 0xFF);
    }
    o->size = GT_CANVAS * GT_CANVAS * 4;
    graphtool.save_status = 1;
    desktop_scan_files();
}

int graphtool_handle_key(char key) {
    if (!graphtool.open) return 0;
    if (key == KEY_ESC) { graphtool_close(); return 1; }
    return 0;
}

int graphtool_handle_click(int mx, int my, int left_down, int left_prev) {
    if (!graphtool.open) return 0;
    gt_recalc_layout();

    int cw = GT_CANVAS * GT_PIXEL;
    int inside_canvas = (mx >= graphtool.canvas_x && mx < graphtool.canvas_x + cw &&
                         my >= graphtool.canvas_y && my < graphtool.canvas_y + cw);

    int px = (mx - graphtool.canvas_x) / GT_PIXEL;
    int py = (my - graphtool.canvas_y) / GT_PIXEL;

    int just_pressed = left_down && !left_prev;
    int just_released = !left_down && left_prev;

    if (just_pressed) {
        // Кнопки инструментов (7 штук)
        if (my >= graphtool.tool_btn_y && my < graphtool.tool_btn_y + 25) {
            for (int i = 0; i < 7; i++) {
                if (mx >= graphtool.tool_btn_x[i] && mx < graphtool.tool_btn_x[i] + 36) {
                    graphtool.current_tool = i;
                    return 1;
                }
            }
        }
        // Кнопки толщины
        if (my >= graphtool.thick_btn_y && my < graphtool.thick_btn_y + 25) {
            for (int i = 0; i < 5; i++) {
                if (mx >= graphtool.thick_btn_x[i] && mx < graphtool.thick_btn_x[i] + 16) {
                    graphtool.brush_size = i + 1;
                    return 1;
                }
            }
        }
        // Палитра
        if (mx >= graphtool.palette_x && my >= graphtool.palette_y) {
            int per_row = GT_COLORS / 2;
            int col = (mx - graphtool.palette_x) / GT_SWATCH;
            int row = (my - graphtool.palette_y) / GT_SWATCH;
            if (col >= 0 && col < per_row && row >= 0 && row < 2) {
                graphtool.current_color = row * per_row + col;
                return 1;
            }
        }
        // Очистить и Сохранить
        if (mx >= graphtool.btn_clear_x && mx < graphtool.btn_clear_x + GT_BTN_W && my >= graphtool.btn_clear_y && my < graphtool.btn_clear_y + GT_BTN_H) { graphtool_clear(); return 1; }
        if (mx >= graphtool.btn_save_x && mx < graphtool.btn_save_x + GT_BTN_W && my >= graphtool.btn_save_y && my < graphtool.btn_save_y + GT_BTN_H) { graphtool_save(); return 1; }

        if (inside_canvas && px >= 0 && px < GT_CANVAS && py >= 0 && py < GT_CANVAS) {
            graphtool.start_x = px;
            graphtool.start_y = py;
            graphtool.mouse_was_down = 1;
            graphtool.save_status = 0;

            // Если выбрана ЗАЛИВКА - сразу заливаем и не ждем движения
            if (graphtool.current_tool == TOOL_FILL) {
                flood_fill(graphtool.pixels, px, py, graphtool.palette[graphtool.current_color]);
                graphtool.mouse_was_down = 0;
                return 1;
            }

            for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) graphtool.preview_pixels[i] = graphtool.pixels[i];

            if (graphtool.current_tool == TOOL_BRUSH) {
                set_pixel(graphtool.pixels, px, py, graphtool.palette[graphtool.current_color], graphtool.brush_size);
            }
            return 1;
        }
    }

    // Движение мыши
    if (left_down && graphtool.mouse_was_down && inside_canvas && px >= 0 && px < GT_CANVAS && py >= 0 && py < GT_CANVAS) {
        if (graphtool.current_tool == TOOL_BRUSH) {
            draw_line(graphtool.pixels, graphtool.start_x, graphtool.start_y, px, py, graphtool.palette[graphtool.current_color], graphtool.brush_size);
            graphtool.start_x = px; graphtool.start_y = py;
        } else if (graphtool.current_tool != TOOL_FILL) {
            // Предпросмотр
            for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) graphtool.preview_pixels[i] = graphtool.pixels[i];

            uint32_t col = graphtool.palette[graphtool.current_color];
            int sz = graphtool.brush_size;

            if (graphtool.current_tool == TOOL_LINE) draw_line(graphtool.preview_pixels, graphtool.start_x, graphtool.start_y, px, py, col, sz);
            if (graphtool.current_tool == TOOL_RECT) draw_rect(graphtool.preview_pixels, graphtool.start_x, graphtool.start_y, px, py, col, sz);
            if (graphtool.current_tool == TOOL_OVAL) {
                int rx = (px > graphtool.start_x) ? (px - graphtool.start_x) : (graphtool.start_x - px);
                int ry = (py > graphtool.start_y) ? (py - graphtool.start_y) : (graphtool.start_y - py);
                draw_oval(graphtool.preview_pixels, graphtool.start_x, graphtool.start_y, rx, ry, col, sz);
            }
            if (graphtool.current_tool == TOOL_TRIANGLE) draw_triangle(graphtool.preview_pixels, graphtool.start_x, graphtool.start_y, px, py, col, sz);
            if (graphtool.current_tool == TOOL_HEXAGON) {
                int r = (px > graphtool.start_x) ? (px - graphtool.start_x) : (graphtool.start_x - px);
                draw_hexagon(graphtool.preview_pixels, graphtool.start_x, graphtool.start_y, r, col, sz);
            }
        }
        return 1;
    }

    // Отпускание
    if (just_released && graphtool.mouse_was_down) {
        graphtool.mouse_was_down = 0;
        if (graphtool.current_tool != TOOL_BRUSH && graphtool.current_tool != TOOL_FILL && inside_canvas && px >= 0 && px < GT_CANVAS && py >= 0 && py < GT_CANVAS) {
            for (int i = 0; i < GT_CANVAS * GT_CANVAS; i++) graphtool.pixels[i] = graphtool.preview_pixels[i];
            return 1;
        }
    }

    return 0;
}

// --- Отрисовка ---
static void gt_char(GraphTool* g, int x, int y, char c, uint32_t fg, uint32_t bg) {
    unsigned char ch = (unsigned char)c;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[ch][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            int px = x + col; int py = y + row;
            if (px < g->win_x || px >= g->win_x + g->win_w) continue;
            if (py < g->win_y + WM_TITLE_H || py >= g->win_y + g->win_h) continue;
            if (px < 0 || px >= g->buf_w || py < 0 || py >= g->buf_h) continue;
            g->buf[py * g->buf_w + px] = color;
        }
    }
}
static void gt_string(GraphTool* g, int x, int y, const char* s, uint32_t fg, uint32_t bg) {
    int cx = x; while (*s) { gt_char(g, cx, y, *s, fg, bg); cx += 8; s++; }
}
static void gt_fill_rect(GraphTool* g, int x, int y, int w, int h, uint32_t c) {
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            if (px >= g->win_x && px < g->win_x + g->win_w && py >= g->win_y + WM_TITLE_H && py < g->win_y + g->win_h)
                if (px >= 0 && px < g->buf_w && py >= 0 && py < g->buf_h) g->buf[py * g->buf_w + px] = c;
}

void graphtool_render(GraphTool* g) {
    if (!g->open || !g->buf) return;

    for (int y = g->win_y + WM_TITLE_H; y < g->win_y + g->win_h; y++) {
        for (int x = g->win_x + WM_BORDER; x < g->win_x + g->win_w - WM_BORDER; x++) {
            if (x >= 0 && x < g->buf_w && y >= 0 && y < g->buf_h) g->buf[y * g->buf_w + x] = 0x2A2A35;
        }
    }

    uint32_t* active_arr = g->mouse_was_down ? g->preview_pixels : g->pixels;
    if (g->current_tool == TOOL_BRUSH || g->current_tool == TOOL_FILL) active_arr = g->pixels;

    for (int py = 0; py < GT_CANVAS; py++) {
        for (int px = 0; px < GT_CANVAS; px++) {
            uint32_t c = active_arr[py * GT_CANVAS + px];
            for (int dy = 0; dy < GT_PIXEL; dy++) {
                for (int dx = 0; dx < GT_PIXEL; dx++) {
                    int fx = g->canvas_x + px * GT_PIXEL + dx;
                    int fy = g->canvas_y + py * GT_PIXEL + dy;
                    if (fx >= g->win_x && fx < g->win_x + g->win_w && fy >= g->win_y + WM_TITLE_H && fy < g->win_y + g->win_h)
                        if (fx >= 0 && fx < g->buf_w && fy >= 0 && fy < g->buf_h) g->buf[fy * g->buf_w + fx] = c;
                }
            }
        }
    }

    // Верхняя панель
    gt_fill_rect(g, g->win_x + 2, g->win_y + WM_TITLE_H, g->win_w - 4, 35, 0x1A1A2A);

    // 7 кнопок инструментов
    const char* t_names[] = {"Br", "Ln", "Rc", "Ov", "Tr", "Hx", "Fl"}; // Fl = Fill
    for (int i = 0; i < 7; i++) {
        uint32_t c = (i == g->current_tool) ? 0x3050C0 : 0x404060;
        gt_fill_rect(g, g->tool_btn_x[i], g->tool_btn_y, 36, 25, c);
        gt_string(g, g->tool_btn_x[i] + 10, g->tool_btn_y + 5, t_names[i], 0xFFFFFF, c);
    }

    // Толщина
    gt_string(g, g->thick_btn_x[0] - 35, g->thick_btn_y + 5, "Sz", 0xFFFFFF, 0x1A1A2A);
    for (int i = 0; i < 5; i++) {
        uint32_t c = (i + 1 == g->brush_size) ? 0x3050C0 : 0x404060;
        gt_fill_rect(g, g->thick_btn_x[i], g->thick_btn_y, 16, 25, c);
        char b[2] = { (char)('1' + i), 0 };
        gt_string(g, g->thick_btn_x[i] + 5, g->thick_btn_y + 5, b, 0xFFFFFF, c);
    }

    // Палитра
    int per_row = GT_COLORS / 2;
    for (int i = 0; i < GT_COLORS; i++) {
        int row = i / per_row, col = i % per_row;
        int sx = g->palette_x + col * GT_SWATCH;
        int sy = g->palette_y + row * GT_SWATCH;
        gt_fill_rect(g, sx, sy, GT_SWATCH - 2, GT_SWATCH - 2, g->palette[i]);
        if (i == g->current_color) {
            for (int t = 0; t < GT_SWATCH - 2; t++) {
                if (sx+t < g->buf_w && sy+t < g->buf_h) g->buf[sy * g->buf_w + sx+t] = 0xFFFFFF;
                if (sx+t < g->buf_w && sy+GT_SWATCH-3 < g->buf_h) g->buf[(sy+GT_SWATCH-3) * g->buf_w + sx+t] = 0xFFFFFF;
            }
        }
    }

    // Кнопки
    gt_fill_rect(g, g->btn_clear_x, g->btn_clear_y, GT_BTN_W, GT_BTN_H, 0x8B2020);
    gt_string(g, g->btn_clear_x + 6, g->btn_clear_y + 5, "Clear", 0xFFFFFF, 0x8B2020);
    gt_fill_rect(g, g->btn_save_x, g->btn_save_y, GT_BTN_W, GT_BTN_H, 0x206B20);
    gt_string(g, g->btn_save_x + 6, g->btn_save_y + 5, "Save", 0xFFFFFF, 0x206B20);

    // Результат сохранения
    int sx = g->btn_clear_x + GT_BTN_W + 12, sy = g->btn_save_y + 5;
    if (g->save_status == 1)       gt_string(g, sx, sy, "Saved", 0x206B20, 0xFFFFFF);
    else if (g->save_status == -1) gt_string(g, sx, sy, "Save failed: no space", 0xC03030, 0xFFFFFF);
    else if (g->save_status == -2) gt_string(g, sx, sy, "No Desktop folder", 0xC03030, 0xFFFFFF);
}

void graphtool_render_for_window(int wx, int wy, int ww, int wh) {
    if (!graphtool.open) return;
    graphtool.win_x = wx; graphtool.win_y = wy; graphtool.win_w = ww; graphtool.win_h = wh;
    gt_recalc_layout();
    graphtool_render(&graphtool);
}