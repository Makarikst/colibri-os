#ifndef WM_H
#define WM_H

#include "../framebuffer/framebuffer.h"
#include "../utils/utils.h"

/* ============================================
   Оконный менеджер
   ============================================ */

#define WM_MAX_WINDOWS  8
#define WM_TITLE_H      24
#define WM_BORDER       2

/* Тип окна */
#define WM_TYPE_EMPTY     0
#define WM_TYPE_TERMINAL  1
#define WM_TYPE_NANO      2
#define WM_TYPE_FILER     3
#define WM_TYPE_GRAPHTOOL 4

typedef struct {
    int  active;
    int  x, y;
    int  w, h;
    int  type;
    const char* title;
    int  dragging;
    int  drag_dx, drag_dy;
    void* user_data;
} Window;

void wm_init(void);

int  wm_create(int x, int y, int w, int h, int type, const char* title);
void wm_close(int idx);

void wm_update(int mouse_x, int mouse_y, int left_down, int left_prev);

/* content_cb вызывается для каждого окна по Z-порядку */
void wm_draw_all(uint32_t* buf, int bw, int bh,
                 void (*content_cb)(int type, int wx, int wy, int ww, int wh));

Window* wm_get(int idx);

#endif