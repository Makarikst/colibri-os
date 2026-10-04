#ifndef NANO_DESK_H
#define NANO_DESK_H

#include "../framebuffer/framebuffer.h"
#include "../fs/fs.h"
#include "../utils/utils.h"
#include "../kbd/kbd.h"

#define NANO_BUF_SIZE  4000
#define NANO_FONT_W    8
#define NANO_FONT_H    16

typedef struct {
    int  open;
    int  win_idx;
    int  cur_x, cur_y;      /* верхний левый угол текстовой области */
    int  win_x, win_y, win_w, win_h;

    /* Буфер текста */
    char buf[NANO_BUF_SIZE];
    int  len;               /* сколько символов занято */
    int  cursor;            /* позиция курсора */

    /* Имя файла в FS (-1 — новый) */
    int  file_fs_index;

    /* Строка статуса (сообщение) */
    char status[64];
    int  status_timer;

    uint32_t* buf_pixels;
    int  bw, bh;
} NanoEditor;

extern NanoEditor nano;

void nano_desk_init(void);
void nano_desk_open_empty(int win_idx, int x, int y, int w, int h);
void nano_desk_open_file(int fs_index, int win_idx, int x, int y, int w, int h);
void nano_desk_close(void);
void nano_desk_handle_key(char key);
void nano_desk_render(NanoEditor* n);
void nano_desk_render_for_window(int wx, int wy, int ww, int wh);

#endif