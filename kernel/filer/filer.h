#ifndef FILER_H
#define FILER_H

#include "../framebuffer/framebuffer.h"
#include "../fs/fs.h"
#include "../utils/utils.h"
#include "../kbd/kbd.h"

#define FILER_MAX_ITEMS   64
#define FILER_ITEM_H      20
#define FILER_HEADER_H    30

typedef struct { /* счётчик, когда был прошлый клик */
    int  open;
    int  win_idx;
    int  cur_x, cur_y;
    int  width, height;

    int  win_x, win_y, win_w, win_h;

    int  current_dir;
    int  item_count;
    int  selected;

    /* Индекс файла в FS, выбранного для открытия */
    int  selected_file_index;

    int  item_indices[FILER_MAX_ITEMS];

    int  up_btn_x, up_btn_y, up_btn_w, up_btn_h;

    /* === Переименование === */
    int  rename_mode;                /* 1 = редактируем имя */
    int  rename_row;                 /* какую строку переименовываем */
    char rename_buf[FS_NAME_LEN];    /* буфер нового имени */
    int  rename_len;                 /* длина в буфере */

    uint32_t* buf;
    int  buf_w, buf_h;
} Filer;

/* Глобальный экземпляр */
extern Filer filer;

void filer_init(Filer* f);
void filer_open(Filer* f, int win_idx, int x, int y, int w, int h);
void filer_close(Filer* f);
void filer_reload(Filer* f);
int filer_handle_click(Filer* f, int mx, int my);
int filer_handle_enter(Filer* f);   /* обработать Enter: зайти в папку или открыть файл */
void filer_render(Filer* f);

/* === Переименование === */
void filer_start_rename(Filer* f);
int  filer_handle_key(Filer* f, char key);

/* Специально для wm_draw_all */
void filer_render_for_window(int win_x, int win_y, int win_w, int win_h);

#endif