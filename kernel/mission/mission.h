#ifndef MISSION_H
#define MISSION_H

#include "../utils/utils.h"

void mission_toggle(void);
void mission_open(void);
void mission_close(void);
int  mission_is_open(void);

void mission_render(uint32_t* buf, int bw, int bh);
int  mission_handle_click(int mx, int my, int left, int prev);
int  mission_handle_key(char k);

#endif