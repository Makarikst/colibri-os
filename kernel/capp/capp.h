#ifndef CAPP_H
#define CAPP_H

#include "../utils/utils.h"

// Запуск кастомных Cx-приложений (.capp) в окнах оконного менеджера.

void capp_init(void);
void capp_set_buffer(uint32_t* buf, int w, int h);

// Открыть приложение из файла fs_idx. 1 — окно открыто (или уже было), 0 — не удалось.
int  capp_open(int fs_idx);

// Освободить приложения, чьи окна закрыты. 1 — что-то изменилось (нужна перерисовка).
int  capp_gc(void);

// Вызывается из window_content_cb для WM_TYPE_CAPP.
void capp_render_for_window(int wx, int wy, int ww, int wh);

#endif