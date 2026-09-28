#ifndef KBD_H
#define KBD_H

#include "../utils/utils.h"

/* ============================================
   Клавиатура (PS/2, порты 0x60 / 0x64)
   ============================================ */

/* Специальные клавиши (отрицательные — не конфликтуют с ASCII) */
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

/* --- Инициализация --- */
void kbd_init(void);

/* --- Получить нажатие (блокирующий) --- */
char kbd_get_key(void);

/* --- Проверить состояние Shift/Ctrl (для внешних модулей) --- */
int kbd_shift_pressed(void);
int kbd_ctrl_pressed(void);

#endif