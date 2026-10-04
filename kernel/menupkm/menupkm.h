#ifndef MENUPKM_H
#define MENUPKM_H

#include "../framebuffer/framebuffer.h"
#include "../utils/utils.h"

/* ============================================
   Контекстное меню (ПКМ)
   ============================================ */

#define MENU_MAX_ITEMS     8
#define MENU_ITEM_H        24    /* высота пункта */
#define MENU_WIDTH         180   /* ширина меню */
#define MENU_TEXT_PAD_X    10    /* отступ текста */
#define MENU_TEXT_PAD_Y    4

/* Цвета */
#define MENU_BG            0x303030
#define MENU_BORDER        0x101010
#define MENU_TEXT          0xFFFFFF
#define MENU_HOVER_BG      0x5050A0
#define MENU_SEPARATOR     0x606060

/* Идентификаторы действий */
#define MENU_ACTION_NONE        0
#define MENU_ACTION_NEW_FILE    1
#define MENU_ACTION_NEW_FOLDER  2
#define MENU_ACTION_OPEN        3
#define MENU_ACTION_DELETE      4
#define MENU_ACTION_CANCEL      5

typedef struct {
    int  open;
    int  x, y;                      /* верхний левый угол меню */
    int  w;                          /* ширина (MENU_WIDTH) */
    int  h;                          /* полная высота = count * MENU_ITEM_H */

    int  count;
    const char* labels[MENU_MAX_ITEMS];
    int  actions[MENU_MAX_ITEMS];    /* MENU_ACTION_* */

    /* Что "под меню" (для последующего выполнения действия) */
    int  target_icon;                /* индекс ярлыка или -1, если пустое место */
} ContextMenu;

void menu_init(ContextMenu* m);

/* Открыть меню в (x, y) с указанными пунктами */
void menu_open(ContextMenu* m, int x, int y,
               const char* labels[], const int actions[], int count,
               int target_icon);

void menu_close(ContextMenu* m);

/* Обновление: обработка мыши.
   Возвращает действие (MENU_ACTION_*) или MENU_ACTION_NONE */
int menu_update(ContextMenu* m, int mx, int my, int left_down, int left_prev);

/* Отрисовка в буфер */
void menu_draw(ContextMenu* m, uint32_t* buf, int bw, int bh);

#endif