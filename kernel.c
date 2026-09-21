/* ============================================
   Colibri OS — Ядро v0.5
   VGA + клавиатура + Shell + ФС + куча
   + nano + .cpe (интерпретатор) + .cai (заглушка)
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
#define LIGHT_RED    12

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

static void print_dec(unsigned int n) {
    if (n == 0) { putchar('0'); return; }
    char buf[12];
    int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) putchar(buf[--i]);
}

/* ---------- Клавиатура ---------- */
/* Обычная раскладка (без Shift) */
/* Обычная раскладка (без Shift) */
static const char kbd_map_lower[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0,  ' '
};

/* Раскладка с Shift */
static const char kbd_map_upper[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',
    0,  '*', 0,  ' '
};

static int shift_pressed = 0;
static int ctrl_pressed = 0;

/* Специальные коды для Ctrl-комбинаций */
#define KEY_CTRL_Q  1
#define KEY_CTRL_S  2
#define KEY_ESC     27

char get_key() {
    while (1) {
        if (inb(0x64) & 0x01) {
            unsigned char sc = inb(0x60);

            /* Отпускание клавиши */
            if (sc & 0x80) {
                unsigned char released = sc & 0x7F;
                if (released == 0x2A || released == 0x36) shift_pressed = 0;
                if (released == 0x1D) ctrl_pressed = 0;
                continue;
            }

            /* Нажатие Shift */
            if (sc == 0x2A || sc == 0x36) {
                shift_pressed = 1;
                continue;
            }

            /* Нажатие Ctrl */
            if (sc == 0x1D) {
                ctrl_pressed = 1;
                continue;
            }

            /* Обычные клавиши */
            if (sc < 128) {
                char c = shift_pressed ? kbd_map_upper[sc] : kbd_map_lower[sc];

                /* Обработка Ctrl-комбинаций */
                if (ctrl_pressed) {
                    if (c == 'q' || c == 'Q') return KEY_CTRL_Q;
                    if (c == 's' || c == 'S') return KEY_CTRL_S;
                }

                if (c) return c;
            }
        }
    }
}

/* ---------- Файловая система (в RAM) ---------- */
#define FS_MAX_FILES 10
#define FS_NAME_LEN  32
#define FS_DATA_LEN  4096

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
        fs_files[i].data[0] = 0;
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

int strlen(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
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

void fs_list() {
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_files[i].used) {
            print("  ");
            print(fs_files[i].name);
            print("  (");
            print_dec(fs_files[i].size);
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
    File* f = fs_find(filename);

    if (!f) {
        print_color("nano: file not found: ", LIGHT_RED);
        print(filename);
        putchar('\n');
        print("Use 'touch ");
        print(filename);
        print("' to create it.\n");
        return;
    }

    strcpy(nano_filename, filename);
    for (int i = 0; i < f->size; i++) nano_buffer[i] = f->data[i];
    nano_buffer[f->size] = 0;
    nano_size = f->size;

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

        if (c == KEY_CTRL_Q) {  /* Ctrl+Q */
            clear_screen();
            return;
        }
        if (c == KEY_CTRL_S) {  /* Ctrl+S */
            for (int i = 0; i < pos; i++) f->data[i] = nano_buffer[i];
            f->data[pos] = 0;
            f->size = pos;
            print_color("\n[saved ", LIGHT_GREEN);
            print_dec(pos);
            print_color(" bytes]\n", LIGHT_GREEN);
            continue;
        }
        if (c == KEY_ESC) {  /* Esc — тоже выход */
            clear_screen();
            return;
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

/* ============================================
   .cpe — простой Python-подобный интерпретатор
   Поддерживает:
     print("text")
     print(variable)
     x = 5
     x = "string"
   ============================================ */

/* Переменные */
#define CPE_MAX_VARS 16
#define CPE_VAR_NAME 32
#define CPE_VAR_VAL  128

typedef struct {
    char name[CPE_VAR_NAME];
    char value[CPE_VAR_VAL];
    int  is_string;
} CpeVar;

static CpeVar cpe_vars[CPE_MAX_VARS];
static int    cpe_var_count = 0;

void cpe_vars_reset() {
    cpe_var_count = 0;
    for (int i = 0; i < CPE_MAX_VARS; i++) {
        cpe_vars[i].name[0] = 0;
        cpe_vars[i].value[0] = 0;
        cpe_vars[i].is_string = 0;
    }
}

CpeVar* cpe_find_var(const char* name) {
    for (int i = 0; i < cpe_var_count; i++) {
        if (strcmp(cpe_vars[i].name, name) == 0) return &cpe_vars[i];
    }
    return 0;
}

void cpe_set_var(const char* name, const char* value, int is_string) {
    CpeVar* v = cpe_find_var(name);
    if (!v) {
        if (cpe_var_count >= CPE_MAX_VARS) return;
        v = &cpe_vars[cpe_var_count++];
        strcpy(v->name, name);
    }
    strcpy(v->value, value);
    v->is_string = is_string;
}

/* Пропустить пробелы */
const char* cpe_skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Прочитать строку в кавычках */
const char* cpe_read_string(const char* p, char* out) {
    p = cpe_skip_ws(p);
    if (*p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < CPE_VAR_VAL - 1) {
        out[i++] = *p++;
    }
    out[i] = 0;
    if (*p == '"') p++;
    return p;
}

/* Прочитать идентификатор/число */
const char* cpe_read_token(const char* p, char* out) {
    p = cpe_skip_ws(p);
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
           *p != '(' && *p != ')' && *p != '=' && *p != '"' &&
           i < CPE_VAR_VAL - 1) {
        out[i++] = *p++;
    }
    out[i] = 0;
    return p;
}

/* Выполнить одну строку */
void cpe_exec_line(const char* line) {
    const char* p = cpe_skip_ws(line);

    /* Пропускаем пустые строки и комментарии */
    if (*p == 0 || *p == '#') return;

    /* print(...) */
    if (strncmp(p, "print", 5) == 0) {
        p = cpe_skip_ws(p + 5);
        if (*p == '(') {
            p++;
            p = cpe_skip_ws(p);

            if (*p == '"') {
                char buf[CPE_VAR_VAL];
                p = cpe_read_string(p, buf);
                print(buf);
                putchar('\n');
            } else {
                char token[CPE_VAR_VAL];
                p = cpe_read_token(p, token);
                if (token[0]) {
                    CpeVar* v = cpe_find_var(token);
                    if (v) {
                        print(v->value);
                        putchar('\n');
                    } else {
                        print(token);
                        putchar('\n');
                    }
                }
            }
        }
        return;
    }

    /* Присваивание: name = value */
    char name[CPE_VAR_NAME];
    const char* q = cpe_read_token(p, name);
    q = cpe_skip_ws(q);
    if (*q == '=') {
        q++;
        q = cpe_skip_ws(q);

        if (*q == '"') {
            char buf[CPE_VAR_VAL];
            q = cpe_read_string(q, buf);
            cpe_set_var(name, buf, 1);
        } else {
            char buf[CPE_VAR_VAL];
            q = cpe_read_token(q, buf);
            cpe_set_var(name, buf, 0);
        }
    }
}

/* Запустить .cpe файл */
void cpe_run(const char* filename) {
    File* f = fs_find(filename);

    if (!f) {
        print_color("cpe: file not found: ", LIGHT_RED);
        print(filename);
        putchar('\n');
        return;
    }

    cpe_vars_reset();

    char line[256];
    int line_len = 0;

    for (int i = 0; i < f->size; i++) {
        char c = f->data[i];
        if (c == '\n' || line_len >= 255) {
            line[line_len] = 0;
            cpe_exec_line(line);
            line_len = 0;
        } else {
            line[line_len++] = c;
        }
    }
    if (line_len > 0) {
        line[line_len] = 0;
        cpe_exec_line(line);
    }
}

/* ---------- .cai (заглушка v1.0) ---------- */
void cai_run(const char* filename) {
    print_color("cai: ", LIGHT_YELLOW);
    print(filename);
    putchar('\n');
    print("CAI runtime will be implemented in v1.0.\n");
    print("Format: header + bytecode + resources.\n");
    print("Note: .cai must be installed before running.\n");
}

/* ---------- Shell ---------- */
static char cmd_buffer[128];

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
        if (i < 127) {
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
    print("  cpe X        - run .cpe script (Python-like)\n");
    print("  run X        - run .cai application (v1.0)\n");
    print("  heap         - heap statistics\n");
    print("  reboot       - reboot\n");
}

void cmd_about() {
    print("Colibri OS - hobby OS by Asde LLC\n");
    print("Written in C, running in 32-bit mode\n");
}

void cmd_ver() {
    print("Colibri OS v0.5 (heap + nano + cpe interpreter)\n");
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

    if (strncmp(cmd_buffer, "cpe ", 4) == 0) {
        const char* name = cmd_buffer + 4;
        if (name[0] == 0) { print("Usage: cpe <file.cpe>\n"); return; }
        cpe_run(name);
        return;
    }

    if (strncmp(cmd_buffer, "run ", 4) == 0) {
        const char* name = cmd_buffer + 4;
        if (name[0] == 0) { print("Usage: run <file.cai>\n"); return; }
        cai_run(name);
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
    print_color("Colibri OS v0.5\n", LIGHT_CYAN);
    print_color("================\n", LIGHT_CYAN);
    print("Heap + nano + cpe interpreter ready.\n");
    print("Type 'help' for commands.\n\n");
}

void kernel_main() {
    clear_screen();
    heap_init();
    fs_init();
    print_banner();
    shell_loop();
}