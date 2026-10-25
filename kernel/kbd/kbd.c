#include "kbd.h"

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

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int extended = 0;
static int alt_pressed = 0;
static int super_pressed = 0; // Для Cmd/Win

void kbd_init(void) {
    while (inb(0x64) & 0x01) { inb(0x60); }
    shift_pressed = 0;
    ctrl_pressed = 0;
    extended = 0;
    alt_pressed = 0;
    super_pressed = 0;
}

char kbd_get_key(void) {
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return 0;
    if (status & 0x20)   return 0;

    uint8_t sc = inb(0x60);

    if (sc == 0xE0) { extended = 1; return 0; }

    if (sc & 0x80) { // Кнопка отпущена
        unsigned char r = sc & 0x7F;
        if (r == 0x2A || r == 0x36) shift_pressed = 0;
        if (r == 0x1D) ctrl_pressed = 0;
        if (r == 0x38) alt_pressed = 0;
        if (r == 0x5B || r == 0x5C) super_pressed = 0; // Отпускание Cmd/Win
        extended = 0;
        return 0;
    }

    // Нажатие Super (Cmd/Win)
    if (sc == 0x5B || sc == 0x5C) { super_pressed = 1; return 0; }

    if (extended) {
        extended = 0;

        // Super + Стрелки (Workspaces)
        if (super_pressed) {
            if (sc == 0x4B) return KEY_SUPER_LEFT;
            if (sc == 0x4D) return KEY_SUPER_RIGHT;
        }

        // Alt + Стрелки (дублируем на случай, если Super не сработает)
        if (alt_pressed) {
            if (sc == 0x4B) return KEY_SUPER_LEFT;
            if (sc == 0x4D) return KEY_SUPER_RIGHT;
        }

        // Обычные стрелки
        if (sc == 0x48) return KEY_UP;
        if (sc == 0x50) return KEY_DOWN;
        if (sc == 0x4B) return KEY_LEFT;
        if (sc == 0x4D) return KEY_RIGHT;
        if (sc == 0x49) return KEY_PAGEUP;
        if (sc == 0x51) return KEY_PAGEDOWN;
        return 0;
    }

    if (sc == 0x0F) { if (alt_pressed) return KEY_ALT_TAB; return KEY_TAB; }
    if (sc == 0x57) return KEY_F11;

    if (sc == 0x38) { alt_pressed = 1; return 0; }
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 0; }
    if (sc == 0x1D) { ctrl_pressed = 1; return 0; }

    if (sc < 128) {
        char c = shift_pressed ? kbd_upper[sc] : kbd_lower[sc];
        if (ctrl_pressed) {
            char lower_c = (c >= 'A' && c <= 'Z') ? c + 32 : c;
            if (lower_c == 'c') return KEY_CTRL_C;
            if (lower_c == 'v') return KEY_CTRL_V;
            if (lower_c == 'x') return KEY_CTRL_X;
            if (lower_c == 'a') return KEY_CTRL_A;
            if (lower_c == 'z') return KEY_CTRL_Z;
            if (lower_c == 'y') return KEY_CTRL_Y;
            if (lower_c == 'f') return KEY_CTRL_F;
            if (lower_c == 'l') return KEY_CTRL_L;
            if (lower_c == 'd') return KEY_CTRL_D;
            if (lower_c == 'q') return KEY_CTRL_Q;
            if (lower_c == 's') return KEY_CTRL_S;
        }
        return c;
    }
    return 0;
}

int kbd_shift_pressed(void) { return shift_pressed; }
int kbd_ctrl_pressed(void)  { return ctrl_pressed; }
int kbd_has_key(void) {
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return 0;
    if (status & 0x20)   return 0;
    return 1;
}