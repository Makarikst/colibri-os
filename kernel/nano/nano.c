#include "nano.h"

/* ============================================
   Буфер редактора
   ============================================ */
static char nano_buffer[FS_DATA_LEN];
static int  nano_size = 0;

/* ============================================
   Открыть файл в редакторе
   ============================================ */
void nano_open(const char* filename) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(filename, fs_get_current_dir(), &parent, last)) {
        vga_print("nano: invalid name\n");
        return;
    }

    int idx = fs_find_in(parent, last);
    if (idx == -1) {
        vga_print("nano: file not found: ");
        vga_print(filename);
        vga_putchar('\n');
        vga_print("Use 'touch ");
        vga_print(filename);
        vga_print("' first.\n");
        return;
    }
    if (fs_get(idx)->type != OBJ_FILE) {
        vga_print("nano: not a file\n");
        return;
    }

    FsObject* f = fs_get(idx);

    /* Копируем содержимое в буфер */
    int sz = f->size;
    if (sz > FS_DATA_LEN - 1) sz = FS_DATA_LEN - 1;
    for (int i = 0; i < sz; i++) nano_buffer[i] = f->data[i];
    nano_buffer[sz] = 0;
    nano_size = sz;

    /* Экран редактора */
    vga_clear();
    vga_print_color("nano: ", VGA_LIGHT_CYAN);
    vga_print(filename);
    vga_print("\n");
    vga_print("(Ctrl+S = save, Ctrl+Q = quit)\n");
    vga_print("--------------------------------\n");
    vga_print(nano_buffer);

    int pos = nano_size;

    /* Главный цикл редактирования */
    while (1) {
        char c = kbd_get_key();

        if (c == KEY_CTRL_Q || c == KEY_ESC) {
            vga_clear();
            return;
        }
        if (c == KEY_CTRL_S) {
            f->size = pos;
            for (int i = 0; i < pos; i++) f->data[i] = nano_buffer[i];
            f->data[pos] = 0;
            vga_print_color("\n[saved ", VGA_LIGHT_GREEN);
            vga_print_dec(pos);
            vga_print_color(" bytes]\n", VGA_LIGHT_GREEN);
            continue;
        }
        if (c == '\n') {
            if (pos < FS_DATA_LEN - 1) {
                nano_buffer[pos++] = '\n';
                nano_buffer[pos] = 0;
                vga_putchar('\n');
            }
            continue;
        }
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                nano_buffer[pos] = 0;
                vga_putchar('\b');
            }
            continue;
        }
        if ((unsigned char)c < 32) continue;
        if (pos < FS_DATA_LEN - 1) {
            nano_buffer[pos++] = c;
            nano_buffer[pos] = 0;
            vga_putchar(c);
        }
    }
}