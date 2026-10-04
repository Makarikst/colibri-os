#include "mouse.h"
#include "../framebuffer/framebuffer.h"

/* ============================================
   Состояние
   ============================================ */
MouseState mouse = { 512, 384, 0, 0, 0, 0, 0, 0, 0 };

/* Есть ли у мыши колёсико (IntelliMouse) */
static int has_wheel = 0;

/* ============================================
   Порты 8042
   ============================================ */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

static void mouse_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if ((inb(PS2_STATUS) & 0x02) == 0) return;
}

static int mouse_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 0x01) return 1;
    return 0;
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(PS2_CMD, 0xD4);
    mouse_wait_write();
    outb(PS2_DATA, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(PS2_DATA);
}

/* ============================================
   Инициализация
   ============================================ */
void mouse_init(void) {
    uint8_t status;

    /* Включить вспомогательное устройство */
    mouse_wait_write();
    outb(PS2_CMD, 0xA8);

    /* Прочитать config byte */
    mouse_wait_write();
    outb(PS2_CMD, 0x20);
    status = mouse_read();

    /* IRQ12 off, тактирование мыши и клавы ON */
    status &= ~0x02;
    status &= ~0x20;
    status &= ~0x10;

    mouse_wait_write();
    outb(PS2_CMD, 0x60);
    mouse_wait_write();
    outb(PS2_DATA, status);

    /* Сброс мыши */
    mouse_write(0xFF);
    mouse_read();   /* 0xFA ACK */
    mouse_read();   /* 0xAA */
    mouse_read();   /* 0x00 */

    /* === Включаем IntelliMouse (колёсико) ===
       Магия: Set sample rate 200, 100, 80 → Get device ID → 0x03 */
    mouse_write(0xF3); mouse_read();
    mouse_write(200);  mouse_read();
    mouse_write(0xF3); mouse_read();
    mouse_write(100);  mouse_read();
    mouse_write(0xF3); mouse_read();
    mouse_write(80);   mouse_read();

    mouse_write(0xF2); mouse_read();
    uint8_t dev_id = mouse_read();
    has_wheel = (dev_id == 0x03);

    /* Ещё раз — некоторые мыши отвечают не с первого раза */
    mouse_write(0xF3); mouse_read();
    mouse_write(200);  mouse_read();
    mouse_write(0xF3); mouse_read();
    mouse_write(200);  mouse_read();
    mouse_write(0xF3); mouse_read();
    mouse_write(80);   mouse_read();

    mouse_write(0xF2); mouse_read();
    dev_id = mouse_read();
    if (dev_id == 0x03) has_wheel = 1;

    /* Включить потоковый режим */
    mouse_write(0xF4);
    mouse_read();

    mouse.x = 512;
    mouse.y = 384;
    mouse.left = 0;
    mouse.right = 0;
    mouse.middle = 0;
    mouse.wheel = 0;
    mouse.dx = 0;
    mouse.dy = 0;
    mouse.updated = 0;
}

/* ============================================
   Опрос мыши
   ============================================ */
void mouse_poll(void) {
    static uint8_t cycle = 0;
    static uint8_t bytes[4];

    int limit = 32;
    while (limit-- > 0) {
        uint8_t status = inb(PS2_STATUS);
        if (!(status & 0x01)) break;

        /* Байт от клавиатуры — не трогаем */
        if (!(status & 0x20)) break;

        uint8_t data = inb(PS2_DATA);

        /* Синхронизация: первый байт имеет bit3 = 1 */
        if (cycle == 0 && !(data & 0x08)) continue;

        bytes[cycle++] = data;

        int packet_size = has_wheel ? 4 : 3;
        if (cycle == packet_size) {
            cycle = 0;

            int8_t dx = (int8_t)bytes[1];
            int8_t dy = (int8_t)bytes[2];

            if (bytes[0] & 0x40) dx = 0;
            if (bytes[0] & 0x80) dy = 0;

            dy = -dy;

            mouse.dx = dx;
            mouse.dy = dy;
            mouse.x += dx;
            mouse.y += dy;

            if (mouse.x < 0) mouse.x = 0;
            if (mouse.y < 0) mouse.y = 0;
            if (mouse.x > (int)fb_info.width  - 8) mouse.x = (int)fb_info.width  - 8;
            if (mouse.y > (int)fb_info.height - 12) mouse.y = (int)fb_info.height - 12;

            mouse.left   = (bytes[0] & 0x01) ? 1 : 0;
            mouse.right  = (bytes[0] & 0x02) ? 1 : 0;
            mouse.middle = (bytes[0] & 0x04) ? 1 : 0;

            /* Колёсико */
            if (has_wheel) {
                int8_t w = (int8_t)bytes[3];
                if (w > 0)      mouse.wheel = 1;
                else if (w < 0) mouse.wheel = -1;
                else            mouse.wheel = 0;
            } else {
                mouse.wheel = 0;
            }

            mouse.updated = 1;
        }
    }
}

int mouse_left_pressed(void)   { return mouse.left; }
int mouse_right_pressed(void)  { return mouse.right; }
int mouse_middle_pressed(void) { return mouse.middle; }