#ifndef MOUSE_H
#define MOUSE_H

#include "../utils/utils.h"

typedef struct {
    int x;
    int y;
    int left;
    int right;
    int middle;
    int wheel;          /* +1 вверх, -1 вниз */
    int dx;
    int dy;
    int updated;
} MouseState;

extern MouseState mouse;

void mouse_init(void);
void mouse_poll(void);

int mouse_left_pressed(void);
int mouse_right_pressed(void);
int mouse_middle_pressed(void);

#endif