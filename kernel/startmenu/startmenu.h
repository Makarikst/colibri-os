#ifndef STARTMENU_H
#define STARTMENU_H

#include "../fs/fs.h"
#include "../utils/utils.h"

#define STARTMENU_MAX_ITEMS 8
#define STARTMENU_ITEM_H    48
#define STARTMENU_WIDTH     200

typedef struct {
    int  open;
    int  x, y;
    int  w, h;
    int  count;
    int  fs_indices[STARTMENU_MAX_ITEMS];
    int  hover;
} StartMenu;

void startmenu_init(StartMenu* m);
void startmenu_open(StartMenu* m, int screen_w, int screen_h);
void startmenu_close(StartMenu* m);
int  startmenu_update(StartMenu* m, int mx, int my, int left_down, int left_prev);
void startmenu_draw(StartMenu* m, uint32_t* buf, int bw, int bh);

#endif