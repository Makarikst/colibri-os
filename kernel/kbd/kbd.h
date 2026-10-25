#ifndef KBD_H
#define KBD_H

#include "../utils/utils.h"

#define KEY_CTRL_C (-10)
#define KEY_CTRL_L (-11)
#define KEY_CTRL_D (-12)
#define KEY_ESC    (-13)
#define KEY_CTRL_Q (-14)
#define KEY_CTRL_S (-15)
#define KEY_UP     (-20)
#define KEY_DOWN   (-21)
#define KEY_LEFT   (-22)
#define KEY_RIGHT  (-23)
#define KEY_TAB    (-24)
#define KEY_PAGEUP   (-25)
#define KEY_PAGEDOWN (-26)

// Новые клавиши для WM
#define KEY_F11      (-31)
#define KEY_ALT_TAB  (-32)

// Горячие клавиши
#define KEY_CTRL_A (-40)
#define KEY_CTRL_V (-41)
#define KEY_CTRL_X (-42)
#define KEY_CTRL_Z (-43)
#define KEY_CTRL_Y (-44) // Redo

void kbd_init(void);
char kbd_get_key(void);
int kbd_shift_pressed(void);
int kbd_ctrl_pressed(void);
int kbd_has_key(void);
// В самом конце файла, перед #endif:
#define KEY_CTRL_F     (-50)
#define KEY_SUPER_LEFT  (-51)
#define KEY_SUPER_RIGHT (-52)

#endif