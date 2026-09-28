#include "kbd.h"

/* ============================================
   Таблицы сканов → символы
   ============================================ */
static const char kbd_lower[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/',
    0, '*', 0, ' '
};

static const char kbd_upper[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?',
    0, '*', 0, ' '
};

/* ============================================
   Состояние
   ============================================ */
static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int extended = 0;

/* ============================================
   Инициализация
   ============================================ */
void kbd_init(void) {
    /* Очищаем буфер контроллера клавиатуры */
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }
    shift_pressed = 0;
    ctrl_pressed = 0;
    extended = 0;
}

/* ============================================
   Получить клавишу (блокирующий цикл)
   ============================================ */
char kbd_get_key(void) {
    while (1) {
        if (inb(0x64) & 0x01) {
            unsigned char sc = inb(0x60);

            /* Extended prefix (0xE0) */
            if (sc == 0xE0) { extended = 1; continue; }

            /* Release (bit 7) */
            if (sc & 0x80) {
                unsigned char r = sc & 0x7F;
                if (r == 0x2A || r == 0x36) shift_pressed = 0;
                if (r == 0x1D) ctrl_pressed = 0;
                extended = 0;
                continue;
            }

            /* Extended keys */
            if (extended) {
                extended = 0;
                if (sc == 0x48) return KEY_UP;
                if (sc == 0x50) return KEY_DOWN;
                if (sc == 0x4B) return KEY_LEFT;
                if (sc == 0x4D) return KEY_RIGHT;
                continue;
            }

            /* Special */
            if (sc == 0x0F) return KEY_TAB;

            /* Modifiers */
            if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; continue; }
            if (sc == 0x1D) { ctrl_pressed = 1; continue; }

            /* Обычные символы */
            if (sc < 128) {
                char c = shift_pressed ? kbd_upper[sc] : kbd_lower[sc];
                if (ctrl_pressed) {
                    if (c == 'c' || c == 'C') return KEY_CTRL_C;
                    if (c == 'l' || c == 'L') return KEY_CTRL_L;
                    if (c == 'd' || c == 'D') return KEY_CTRL_D;
                    if (c == 'q' || c == 'Q') return KEY_CTRL_Q;
                    if (c == 's' || c == 'S') return KEY_CTRL_S;
                }
                if (c) return c;
            }
        }
    }
}

/* ============================================
   Состояние модификаторов
   ============================================ */
int kbd_shift_pressed(void) { return shift_pressed; }
int kbd_ctrl_pressed(void)  { return ctrl_pressed; }