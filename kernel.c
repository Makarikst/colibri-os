/* ============================================
   Colibri OS — Ядро v0.4
   VGA + клавиатура + Shell + ФС + куча
   + nano + .cpe (заготовки)
   ============================================ */

#include "kmalloc.h"

#define VGA_MEMORY 0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define BLACK        0
#define WHITE        15
#define LIGHT_GREEN  10
#define LIGHT_CYAN   11
#define LIGHT_YELLOW 14

static int cursor_x = 0;
static int cursor_y = 0;
static unsigned char color = (BLACK << 4) | WHITE;

/* ---------- Порты ввода-вывода ---------- */
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------- VGA ---------- */
void clear_screen() {
    unsigned char* vga = (unsigned char*) VGA_MEMORY;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i * 2]     = ' ';
        vga[i * 2 + 1] = color;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void scroll_screen() {
    unsigned char* vga = (unsigned char*) VGA_MEMORY;
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga[i * 2]     = vga[(i + VGA_WIDTH) * 2];
        vga[i * 2 + 1] = vga[(i + VGA_WIDTH) * 2 + 1];
    }
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga[i * 2]     = ' ';
        vga[i * 2 + 1] = color;
    }
    cursor_y = VGA_HEIGHT - 1;
}

void putchar(char c) {
    unsigned char* vga = (unsigned char*) VGA_MEMORY;

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) scroll_screen();
        return;
    }

    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            int offset = (cursor_y * VGA_WIDTH + cursor_x) * 2;
            vga[offset]     = ' ';
            vga[offset + 1] = color;
        }
        return;
    }

    int offset = (cursor_y * VGA_WIDTH + cursor_x) * 2;
    vga[offset]     = c;
    vga[offset + 1] = color;

    cursor_x++;
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) scroll_screen();
    }
}

void print(const char* str) {
    while (*str) putchar(*str++);
}

void print_color(const char* str, unsigned char col) {
    unsigned char old = color;
    color = (BLACK << 4) | col;
    print(str);
    color = old;
}

/* ---------- Клавиатура ---------- */
static const char kbd_map[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0,  ' '
};

char get_key() {
    while (1) {
        if (inb(0x64) & 0x01) {
            unsigned char sc = inb(0x60);
            if (sc < 128) {
                char c = kbd_map[sc];
                if (c) return c;
            }
        }
    }
}

/* ---------- Файловая система (в RAM) ---------- */
#define FS_MAX_FILES 10
#define FS_NAME_LEN  32
#define FS_DATA_LEN  1024

typedef struct {
    char name[FS_NAME_LEN];
    char data[FS_DATA_LEN];
    int  used;
    int  size;
} File;

static File fs_files[FS_MAX_FILES];

void fs_init() {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_files[i].used = 0;
        fs_files[i].name[0] = 0;
        fs_files[i].size = 0;
    }
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char* a, const char* b, int n) {
    while (n-- && *a && *b && *a == *b) { a++; b++; }
    if (n < 0) return 0;
    return *a - *b;
}

void strcpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = 0;
}

int fs_create(const char* name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!fs_files[i].used) {
            strcpy(fs_files[i].name, name);
            fs_files[i].used = 1;
            fs_files[i].size = 0;
            fs_files[i].data[0] = 0;
            return 1;
        }
    }
    return 0;
}

int fs_delete(const char* name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_files[i].used && strcmp(fs_files[i].name, name) == 0) {
            fs_files[i].used = 0;
            fs_files[i].name[0] = 0;
            fs_files[i].size = 0;
            return 1;
        }
    }
    return 0;
}

File* fs_find(const char* name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_files[i].used && strcmp(fs_files[i].name, name) == 0) {
            return &fs_files[i];
        }
    }
    return 0;
}

/* ---------- Запуск скриптов ---------- */
void cpe_run(const char* filename);  /* прототип */
void cai_run(const char* filename);  /* прототип (заглушка) */

void run_script(const char* filename) {
    /* Определяем расширение */
    int len = 0;
    while (filename[len]) len++;

    if (len >= 4 && filename[len-4] == '.' &&
        filename[len-3] == 'c' &&
        filename[len-2] == 'p' &&
        filename[len-1] == 'e') {
        /* .cpe — прототип */
        cpe_run(filename);
        }
    else if (len >= 4 && filename[len-4] == '.' &&
             filename[len-3] == 'c' &&
             filename[len-2] == 'a' &&
             filename[len-1] == 'i') {
        /* .cai — финальный формат */
        cai_run(filename);
             }
    else {
        print("Unknown script format: ");
        print(filename);
        print("\nSupported: .cpe (prototype), .cai (v1.0)\n");
    }
}

void cai_run(const char* filename) {
    print_color(".cai: ", LIGHT_YELLOW);
    print(filename);
    print("\n");
    print("CAI runtime will be implemented in v1.0.\n");
    print("Format: header + bytecode + resources.\n");
}

void fs_list() {
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_files[i].used) {
            print("  ");
            print(fs_files[i].name);
            print("  (");
            // Размер файла
            char buf[12];
            int n = fs_files[i].size;
            int j = 0;
            if (n == 0) buf[j++] = '0';
            while (n > 0) { buf[j++] = '0' + (n % 10); n /= 10; }
            while (j > 0) putchar(buf[--j]);
            print(" bytes)\n");
            count++;
        }
    }
    if (count == 0) print("  (empty)\n");
}

/* ---------- nano (текстовый редактор) ---------- */
static char nano_buffer[FS_DATA_LEN];
static int  nano_size = 0;
static char nano_filename[FS_NAME_LEN];

void nano_open(const char* filename) {
    strcpy(nano_filename, filename);
    File* f = fs_find(filename);

    if (f) {
        strcpy(nano_buffer, f->data);
        nano_size = f->size;
    } else {
        nano_buffer[0] = 0;
        nano_size = 0;
    }

    clear_screen();
    print_color("nano: ", LIGHT_CYAN);
    print(filename);
    print("\n");
    print("(Ctrl+S = save, Ctrl+Q = quit)\n");
    print("--------------------------------\n");
    print(nano_buffer);

    int pos = nano_size;
    while (1) {
        char c = get_key();

        /* Ctrl+Q — выход */
        if (c == 17) {
            clear_screen();
            return;
        }
        /* Ctrl+S — сохранение */
        if (c == 19) {
            File* file = fs_find(nano_filename);
            if (!file) {
                fs_create(nano_filename);
                file = fs_find(nano_filename);
            }
            if (file) {
                for (int i = 0; i < pos; i++) file->data[i] = nano_buffer[i];
                file->data[pos] = 0;
                file->size = pos;
                print("\n[saved]\n");
            }
            continue;
        }

        if (c == '\n') {
            if (pos < FS_DATA_LEN - 1) {
                nano_buffer[pos++] = '\n';
                nano_buffer[pos] = 0;
                putchar('\n');
            }
            continue;
        }

        if (c == '\b') {
            if (pos > 0) {
                pos--;
                nano_buffer[pos] = 0;
                putchar('\b');
            }
            continue;
        }

        if (pos < FS_DATA_LEN - 1) {
            nano_buffer[pos++] = c;
            nano_buffer[pos] = 0;
            putchar(c);
        }
    }
}

/* ---------- .cpe (заготовка) ---------- */
void cpe_run(const char* filename) {
    print_color(".cpe: ", LIGHT_YELLOW);
    print(filename);
    print("\n");
    print("CPE runtime not implemented yet.\n");
    print("Coming in v1.0 (built-in Python-like interpreter).\n");
}

/* ---------- Shell ---------- */
static char cmd_buffer[64];

void shell_read_line() {
    int i = 0;
    while (1) {
        char c = get_key();
        if (c == '\n') {
            cmd_buffer[i] = 0;
            putchar('\n');
            return;
        }
        if (c == '\b') {
            if (i > 0) {
                i--;
                putchar('\b');
            }
            continue;
        }
        if (i < 63) {
            cmd_buffer[i++] = c;
            putchar(c);
        }
    }
}

void cmd_help() {
    print("Commands:\n");
    print("  help         - this help\n");
    print("  about        - about Colibri OS\n");
    print("  ver          - version\n");
    print("  clear        - clear screen\n");
    print("  echo X       - print X\n");
    print("  ls           - list files\n");
    print("  touch X      - create file X\n");
    print("  rm X         - delete file X\n");
    print("  nano X       - edit file X\n");
    print("  cpe X        - run .cpe script\n");
    print("  heap         - heap statistics\n");
    print("  reboot       - reboot\n");
}

void cmd_about() {
    print("Colibri OS - hobby OS by Asde LLC\n");
    print("Written in C, running in 32-bit mode\n");
}

void cmd_ver() {
    print("Colibri OS v0.4 (heap + nano + cpe)\n");
}

void cmd_echo(const char* arg) {
    print(arg);
    putchar('\n');
}

void cmd_reboot() {
    print("Rebooting...\n");
    while (inb(0x64) & 0x01) inb(0x60);
    outb(0x64, 0xFE);
}

void parse_command() {
    if (cmd_buffer[0] == 0) return;

    if (strcmp(cmd_buffer, "help") == 0) { cmd_help(); return; }
    if (strcmp(cmd_buffer, "about") == 0) { cmd_about(); return; }
    if (strcmp(cmd_buffer, "ver") == 0) { cmd_ver(); return; }
    if (strcmp(cmd_buffer, "clear") == 0) { clear_screen(); return; }
    if (strcmp(cmd_buffer, "ls") == 0) { fs_list(); return; }
    if (strcmp(cmd_buffer, "reboot") == 0) { cmd_reboot(); return; }
    if (strcmp(cmd_buffer, "heap") == 0) { heap_stats(); return; }

    if (strncmp(cmd_buffer, "echo ", 5) == 0) {
        cmd_echo(cmd_buffer + 5);
        return;
    }

    if (strncmp(cmd_buffer, "touch ", 6) == 0) {
        const char* name = cmd_buffer + 6;
        if (name[0] == 0) { print("Usage: touch <name>\n"); return; }
        if (fs_create(name)) {
            print("File created: ");
            print(name);
            putchar('\n');
        } else {
            print("FS: no free slots!\n");
        }
        return;
    }

    if (strncmp(cmd_buffer, "rm ", 3) == 0) {
        const char* name = cmd_buffer + 3;
        if (name[0] == 0) { print("Usage: rm <name>\n"); return; }
        if (fs_delete(name)) {
            print("File deleted: ");
            print(name);
            putchar('\n');
        } else {
            print("File not found\n");
        }
        return;
    }

    if (strncmp(cmd_buffer, "nano ", 5) == 0) {
        const char* name = cmd_buffer + 5;
        if (name[0] == 0) { print("Usage: nano <file>\n"); return; }
        nano_open(name);
        return;
    }

    if (strncmp(cmd_buffer, "run ", 4) == 0) {
        const char* name = cmd_buffer + 4;
        if (name[0] == 0) { print("Usage: run <file.cpe|file.cai>\n"); return; }
        run_script(name);
        return;
    }

    print("Unknown command: ");
    print(cmd_buffer);
    print("\nType 'help' for list.\n");
}

void shell_loop() {
    while (1) {
        print_color("colibri> ", LIGHT_GREEN);
        shell_read_line();
        parse_command();
    }
}

void print_banner() {
    print_color("Colibri OS v0.4\n", LIGHT_CYAN);
    print_color("================\n", LIGHT_CYAN);
    print("Heap initialized, nano and cpe ready.\n");
    print("Type 'help' for commands.\n\n");
}

void kernel_main() {
    clear_screen();
    heap_init();
    fs_init();
    print_banner();
    shell_loop();
}