#ifndef STUDIO_H
#define STUDIO_H

#include "../framebuffer/framebuffer.h"
#include "../fs/fs.h"

typedef struct {
    int open;
    int win_idx;
    int win_x, win_y, win_w, win_h;
    char code[2048];
    int code_len;
    char output[512];
    int active;

    uint32_t* buf;
    int buf_w, buf_h;
} StudioApp;

extern StudioApp studio;

void studio_init(void);
void studio_open(int win_idx, int x, int y, int w, int h);
void studio_close(void);
void studio_render_for_window(int wx, int wy, int ww, int wh);
int  studio_handle_key(char key);
int  studio_handle_click(int mx, int my, int left_down, int left_prev);

#endif