#ifndef GRAPHTOOL_H
#define GRAPHTOOL_H

#include "../framebuffer/framebuffer.h"
#include "../fs/fs.h"
#include "../utils/utils.h"

#define GT_CANVAS     32
#define GT_PIXEL      6
#define GT_COLORS     16
#define GT_SWATCH     20
#define GT_BTN_W      80
#define GT_BTN_H      26

typedef struct {
    int  open;
    int  win_idx;
    int  win_x, win_y, win_w, win_h;
    int  canvas_x, canvas_y;
    int  palette_x, palette_y;
    int  btn_clear_x, btn_clear_y;
    int  btn_save_x,  btn_save_y;

    uint32_t pixels[GT_CANVAS * GT_CANVAS];
    uint32_t palette[GT_COLORS];
    int  current_color;
    int  file_fs_index;
    int  mouse_was_down;

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
int  graphtool_handle_key(char key);    /* ← НОВОЕ */

void graphtool_clear(void);
void graphtool_save(void);
int  graphtool_load_from_file(int fs_index);

#endif