#ifndef GRAPHTOOL_H
#define GRAPHTOOL_H

#include "../framebuffer/framebuffer.h"
#include "../fs/fs.h"
#include "../utils/utils.h"

#define GT_CANVAS     32
#define GT_PIXEL      6
#define GT_COLORS     16
#define GT_SWATCH     20
#define GT_PALETTE_H  80 // Высота нижней панели
#define GT_BTN_W      80 // <--- ДОБАВИТЬ ЭТО
#define GT_BTN_H      26 // <--- ДОБАВИТЬ ЭТО

// Инструменты
// Инструменты
#define TOOL_BRUSH    0
#define TOOL_LINE     1
#define TOOL_RECT     2
#define TOOL_OVAL     3
#define TOOL_TRIANGLE 4
#define TOOL_HEXAGON  5
#define TOOL_FILL     6 // <--- НОВЫЙ ИНСТРУМЕНТ

typedef struct {
    int  open;
    int  win_idx;
    int  win_x, win_y, win_w, win_h;
    int  canvas_x, canvas_y;
    int  palette_x, palette_y;

    // Координаты кнопок
    int  btn_clear_x, btn_clear_y;
    int  btn_save_x,  btn_save_y;

    uint32_t pixels[GT_CANVAS * GT_CANVAS];
    uint32_t preview_pixels[GT_CANVAS * GT_CANVAS]; // Для предпросмотра фигур
    uint32_t palette[GT_COLORS];
    int  current_color;
    int  file_fs_index;
    int  mouse_was_down;
    int  save_status;       // 0 - нет, 1 - сохранено, -1 - нет места, -2 - нет папки Desktop

    int  current_tool;      // Текущий инструмент
    int  brush_size;        // Толщина (1-5)
    int  start_x, start_y;  // Где начали рисовать

    // Координаты кнопок инструментов и толщины
    int  tool_btn_x[7], tool_btn_y;
    int  thick_btn_x[5], thick_btn_y;

    uint32_t* buf;
    int  buf_w, buf_h;
} GraphTool;

extern GraphTool graphtool;

void graphtool_init(void);
void graphtool_open(int win_idx, int x, int y, int w, int h, int fs_index);
void graphtool_close(void);
int  graphtool_is_open(void);
void graphtool_render(GraphTool* g);
void graphtool_render_for_window(int wx, int wy, int ww, int wh);
int  graphtool_handle_click(int mx, int my, int left_down, int left_prev);
int  graphtool_handle_key(char key);
void graphtool_clear(void);
void graphtool_save(void);
int  graphtool_load_from_file(int fs_index);

#endif