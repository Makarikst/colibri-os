/* ============================================
   Colibri OS — Ядро v0.7c
   VGA + клавиатура + Shell + ФС с каталогами
   + nano + .cpe (input, math, arithmetic)
   + .cai (заглушка)
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

/* ---------- Утилиты строк ---------- */
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

/* ---------- Клавиатура ---------- */
static const char kbd_map_lower[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0,  ' '
};

static const char kbd_map_upper[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',
    0,  '*', 0,  ' '
};

static int shift_pressed = 0;
static int ctrl_pressed = 0;

#define KEY_CTRL_Q  1
#define KEY_CTRL_S  2
#define KEY_ESC     27

char get_key() {
    while (1) {
        if (inb(0x64) & 0x01) {
            unsigned char sc = inb(0x60);

            if (sc & 0x80) {
                unsigned char released = sc & 0x7F;
                if (released == 0x2A || released == 0x36) shift_pressed = 0;
                if (released == 0x1D) ctrl_pressed = 0;
                continue;
            }

            if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; continue; }
            if (sc == 0x1D) { ctrl_pressed = 1; continue; }

            if (sc < 128) {
                char c = shift_pressed ? kbd_map_upper[sc] : kbd_map_lower[sc];

                if (ctrl_pressed) {
                    if (c == 'q' || c == 'Q') return KEY_CTRL_Q;
                    if (c == 's' || c == 'S') return KEY_CTRL_S;
                }
                if (c) return c;
            }
        }
    }
}

/* ============================================
   Файловая система с каталогами (в RAM)
   ============================================ */

#define FS_MAX_OBJECTS 16
#define FS_NAME_LEN    32
#define FS_DATA_LEN    4096

#define OBJ_FREE 0
#define OBJ_FILE 1
#define OBJ_DIR  2

#define ROOT_INDEX 0

typedef struct {
    char name[FS_NAME_LEN];
    char data[FS_DATA_LEN];
    int  type;           /* OBJ_FREE, OBJ_FILE, OBJ_DIR */
    int  parent;         /* индекс родителя, -1 для корня */
    int  size;           /* размер файла */
} FsObject;

static FsObject fs_objects[FS_MAX_OBJECTS];
static int current_dir = ROOT_INDEX;

void fs_init() {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        fs_objects[i].type = OBJ_FREE;
        fs_objects[i].name[0] = 0;
        fs_objects[i].parent = -1;
        fs_objects[i].size = 0;
        fs_objects[i].data[0] = 0;
    }
    /* Корень "/" */
    fs_objects[ROOT_INDEX].type = OBJ_DIR;
    strcpy(fs_objects[ROOT_INDEX].name, "/");
    fs_objects[ROOT_INDEX].parent = -1;
}

/* Найти объект по имени в каталоге parent */
int fs_find_in(int parent, const char* name) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE &&
            fs_objects[i].parent == parent &&
            strcmp(fs_objects[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Создать объект в текущем каталоге */
int fs_create(const char* name, int type) {
    /* Проверяем, нет ли уже такого */
    if (fs_find_in(current_dir, name) != -1) return -1;

    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type == OBJ_FREE) {
            strcpy(fs_objects[i].name, name);
            fs_objects[i].type = type;
            fs_objects[i].parent = current_dir;
            fs_objects[i].size = 0;
            fs_objects[i].data[0] = 0;
            return i;
        }
    }
    return -1;
}

/* Удалить объект по индексу */
void fs_delete_by_index(int idx) {
    if (idx == ROOT_INDEX) return;
    if (fs_objects[idx].type == OBJ_DIR) {
        /* Удаляем всё содержимое */
        for (int i = 0; i < FS_MAX_OBJECTS; i++) {
            if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == idx) {
                fs_delete_by_index(i);
            }
        }
    }
    fs_objects[idx].type = OBJ_FREE;
    fs_objects[idx].name[0] = 0;
    fs_objects[idx].parent = -1;
}

int fs_delete(const char* name) {
    int idx = fs_find_in(current_dir, name);
    if (idx == -1) return 0;
    if (idx == ROOT_INDEX) return 0;
    fs_delete_by_index(idx);
    return 1;
}

/* Список текущего каталога */
void fs_list() {
    int count = 0;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == current_dir) {
            if (fs_objects[i].type == OBJ_DIR) {
                print_color("  ", LIGHT_CYAN);
                print_color(fs_objects[i].name, LIGHT_CYAN);
                print("/\n");
            } else {
                print("  ");
                print(fs_objects[i].name);
                print("  (");
                print_dec(fs_objects[i].size);
                print(" bytes)\n");
            }
            count++;
        }
    }
    if (count == 0) print("  (empty)\n");
}

/* Построить путь текущего каталога */
void fs_pwd() {
    int stack[FS_MAX_OBJECTS];
    int top = 0;
    int cur = current_dir;

    while (cur != ROOT_INDEX) {
        stack[top++] = cur;
        cur = fs_objects[cur].parent;
    }

    print("/");
    while (top > 0) {
        int idx = stack[--top];
        print(fs_objects[idx].name);
        if (top > 0) print("/");
    }
    putchar('\n');
}

/* Перейти в каталог */
int fs_cd(const char* name) {
    if (strcmp(name, "..") == 0) {
        if (current_dir == ROOT_INDEX) return 0;
        current_dir = fs_objects[current_dir].parent;
        return 1;
    }
    if (strcmp(name, "/") == 0) {
        current_dir = ROOT_INDEX;
        return 1;
    }
    int idx = fs_find_in(current_dir, name);
    if (idx == -1 || fs_objects[idx].type != OBJ_DIR) return 0;
    current_dir = idx;
    return 1;
}

/* Получить объект по имени (в текущем каталоге) */
FsObject* fs_find(const char* name) {
    int idx = fs_find_in(current_dir, name);
    if (idx == -1) return 0;
    return &fs_objects[idx];
}

/* ---------- nano ---------- */
static char nano_buffer[FS_DATA_LEN];
static int  nano_size = 0;
static int  nano_index = -1;

void nano_open(const char* filename) {
    int idx = fs_find_in(current_dir, filename);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) {
        print_color("nano: file not found: ", LIGHT_RED);
        print(filename);
        putchar('\n');
        print("Use 'touch ");
        print(filename);
        print("' to create it.\n");
        return;
    }

    nano_index = idx;
    for (int i = 0; i < fs_objects[idx].size; i++) nano_buffer[i] = fs_objects[idx].data[i];
    nano_buffer[fs_objects[idx].size] = 0;
    nano_size = fs_objects[idx].size;

    clear_screen();
    print_color("nano: ", LIGHT_CYAN);
    print(filename);
    print("\n");
    print("(Ctrl+S = save, Ctrl+Q = quit, Esc = quit)\n");
    print("--------------------------------\n");
    print(nano_buffer);

    int pos = nano_size;
    while (1) {
        char c = get_key();

        if (c == KEY_CTRL_Q || c == KEY_ESC) {
            clear_screen();
            return;
        }
        if (c == KEY_CTRL_S) {
            for (int i = 0; i < pos; i++) fs_objects[nano_index].data[i] = nano_buffer[i];
            fs_objects[nano_index].data[pos] = 0;
            fs_objects[nano_index].size = pos;
            print_color("\n[saved ", LIGHT_GREEN);
            print_dec(pos);
            print_color(" bytes]\n", LIGHT_GREEN);
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
            if (pos > 0) { pos--; nano_buffer[pos] = 0; putchar('\b'); }
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
   .cpe — Python-подобный интерпретатор
   ============================================ */

#define CPE_MAX_VARS 32
#define CPE_VAR_NAME 32
#define CPE_VAR_VAL  256

typedef struct {
    char name[CPE_VAR_NAME];
    char value[CPE_VAR_VAL];
    int  is_string;
    int  is_number;
    int  number;
} CpeVar;

static CpeVar cpe_vars[CPE_MAX_VARS];
static int    cpe_var_count = 0;

void cpe_vars_reset() {
    cpe_var_count = 0;
    for (int i = 0; i < CPE_MAX_VARS; i++) {
        cpe_vars[i].name[0] = 0;
        cpe_vars[i].value[0] = 0;
        cpe_vars[i].is_string = 0;
        cpe_vars[i].is_number = 0;
        cpe_vars[i].number = 0;
    }
}

CpeVar* cpe_find_var(const char* name) {
    for (int i = 0; i < cpe_var_count; i++) {
        if (strcmp(cpe_vars[i].name, name) == 0) return &cpe_vars[i];
    }
    return 0;
}

void cpe_set_var(const char* name, const char* value, int is_string, int is_number, int number) {
    CpeVar* v = cpe_find_var(name);
    if (!v) {
        if (cpe_var_count >= CPE_MAX_VARS) return;
        v = &cpe_vars[cpe_var_count++];
        strcpy(v->name, name);
    }
    strcpy(v->value, value);
    v->is_string = is_string;
    v->is_number = is_number;
    v->number = number;
}

const char* cpe_skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

const char* cpe_read_string(const char* p, char* out) {
    p = cpe_skip_ws(p);
    if (*p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < CPE_VAR_VAL - 1) out[i++] = *p++;
    out[i] = 0;
    if (*p == '"') p++;
    return p;
}

const char* cpe_read_token(const char* p, char* out) {
    p = cpe_skip_ws(p);
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
           *p != '(' && *p != ')' && *p != '=' && *p != '"' &&
           *p != '+' && *p != '-' && *p != '*' && *p != '/' &&
           *p != ',' && i < CPE_VAR_VAL - 1) {
        out[i++] = *p++;
    }
    out[i] = 0;
    return p;
}

int cpe_parse_int(const char* s, int* out) {
    int sign = 1;
    int i = 0;
    if (s[0] == '-') { sign = -1; i = 1; }
    if (s[i] == 0) return 0;
    int n = 0;
    while (s[i]) {
        if (s[i] < '0' || s[i] > '9') return 0;
        n = n * 10 + (s[i] - '0');
        i++;
    }
    *out = n * sign;
    return 1;
}

int cpe_get_number(const char* token, int* out) {
    int n;
    if (cpe_parse_int(token, &n)) { *out = n; return 1; }
    CpeVar* v = cpe_find_var(token);
    if (v && v->is_number) { *out = v->number; return 1; }
    return 0;
}

int cpe_eval_expr(const char* expr, int* out) {
    const char* p = expr;
    int have_left = 0;
    int result = 0;
    int op = 0;

    while (*p) {
        p = cpe_skip_ws(p);
        if (*p == 0) break;

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
            op = (*p == '+') ? 1 : (*p == '-') ? 2 : (*p == '*') ? 3 : 4;
            p++;
            continue;
        }

        char token[CPE_VAR_VAL];
        p = cpe_read_token(p, token);
        if (token[0] == 0) break;

        int val;
        if (!cpe_get_number(token, &val)) return 0;

        if (!have_left) { result = val; have_left = 1; }
        else {
            if (op == 1) result += val;
            else if (op == 2) result -= val;
            else if (op == 3) result *= val;
            else if (op == 4) { if (val != 0) result /= val; }
            op = 0;
        }
    }
    if (!have_left) return 0;
    *out = result;
    return 1;
}

static char cpe_input_buffer[CPE_VAR_VAL];

const char* cpe_input(const char* prompt) {
    print(prompt);
    int i = 0;
    while (1) {
        char c = get_key();
        if (c == '\n') {
            cpe_input_buffer[i] = 0;
            putchar('\n');
            return cpe_input_buffer;
        }
        if (c == '\b') {
            if (i > 0) { i--; putchar('\b'); }
            continue;
        }
        if (i < CPE_VAR_VAL - 1) {
            cpe_input_buffer[i++] = c;
            putchar(c);
        }
    }
}

void cpe_exec_line(const char* line) {
    const char* p = cpe_skip_ws(line);
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
                return;
            }

            if (strncmp(p, "math.", 5) == 0) {
                p += 5;
                char fn[16];
                int i = 0;
                while (*p && *p != '(' && i < 15) fn[i++] = *p++;
                fn[i] = 0;
                p = cpe_skip_ws(p);
                if (*p == '(') p++;

                char arg1[CPE_VAR_VAL], arg2[CPE_VAR_VAL];
                int a1 = 0, a2 = 0;

                p = cpe_read_token(p, arg1);
                cpe_get_number(arg1, &a1);

                p = cpe_skip_ws(p);
                if (*p == ',') {
                    p++;
                    p = cpe_read_token(p, arg2);
                    cpe_get_number(arg2, &a2);
                }

                if (strcmp(fn, "sqrt") == 0) {
                    int r = 0;
                    for (int k = 1; k * k <= a1; k++) r = k;
                    print_dec(r);
                } else if (strcmp(fn, "abs") == 0) {
                    print_dec(a1 < 0 ? -a1 : a1);
                } else if (strcmp(fn, "min") == 0) {
                    print_dec(a1 < a2 ? a1 : a2);
                } else if (strcmp(fn, "max") == 0) {
                    print_dec(a1 > a2 ? a1 : a2);
                }
                putchar('\n');
                return;
            }

            char token[CPE_VAR_VAL];
            p = cpe_read_token(p, token);
            if (token[0]) {
                CpeVar* v = cpe_find_var(token);
                if (v) {
                    if (v->is_number) print_dec(v->number);
                    else print(v->value);
                    putchar('\n');
                } else {
                    print(token);
                    putchar('\n');
                }
            }
            return;
        }
    }

    /* input(...) */
    if (strncmp(p, "input", 5) == 0) {
        p = cpe_skip_ws(p + 5);
        if (*p == '(') {
            p++;
            p = cpe_skip_ws(p);
            char prompt[CPE_VAR_VAL];
            if (*p == '"') {
                p = cpe_read_string(p, prompt);
                const char* result = cpe_input(prompt);
                cpe_set_var("_", result, 1, 0, 0);
            }
            return;
        }
    }

    /* Присваивание */
    char name[CPE_VAR_NAME];
    const char* q = cpe_read_token(p, name);
    q = cpe_skip_ws(q);
    if (*q == '=') {
        q++;
        q = cpe_skip_ws(q);

        if (*q == '"') {
            char buf[CPE_VAR_VAL];
            q = cpe_read_string(q, buf);
            cpe_set_var(name, buf, 1, 0, 0);
        } else if (strncmp(q, "input", 5) == 0) {
            q = cpe_skip_ws(q + 5);
            if (*q == '(') {
                q++;
                q = cpe_skip_ws(q);
                char prompt[CPE_VAR_VAL];
                if (*q == '"') {
                    q = cpe_read_string(q, prompt);
                    const char* result = cpe_input(prompt);
                    cpe_set_var(name, result, 1, 0, 0);
                }
            }
        } else {
            int num;
            if (cpe_eval_expr(q, &num)) {
                char buf[16];
                int i = 0;
                if (num == 0) buf[i++] = '0';
                else {
                    char tmp[16]; int ti = 0;
                    int n = num < 0 ? -num : num;
                    while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
                    if (num < 0) buf[i++] = '-';
                    while (ti > 0) buf[i++] = tmp[--ti];
                }
                buf[i] = 0;
                cpe_set_var(name, buf, 0, 1, num);
            } else {
                char buf[CPE_VAR_VAL];
                q = cpe_read_token(q, buf);
                cpe_set_var(name, buf, 0, 0, 0);
            }
        }
    }
}

void cpe_run(const char* filename) {
    int idx = fs_find_in(current_dir, filename);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) {
        print_color("cpe: file not found: ", LIGHT_RED);
        print(filename);
        putchar('\n');
        return;
    }

    cpe_vars_reset();

    char line[256];
    int line_len = 0;

    for (int i = 0; i < fs_objects[idx].size; i++) {
        char c = fs_objects[idx].data[i];
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

/* ---------- .cai (заглушка) ---------- */
void cai_run(const char* filename) {
    print_color("cai: ", LIGHT_YELLOW);
    print(filename);
    putchar('\n');
    print("CAI runtime will be implemented in v1.0.\n");
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
            if (i > 0) { i--; putchar('\b'); }
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
    print("  ls           - list current dir\n");
    print("  pwd          - show current path\n");
    print("  mkdir X      - make directory X\n");
    print("  rmdir X      - remove directory X\n");
    print("  cd X         - change dir (X, .., /)\n");
    print("  touch X      - create file X\n");
    print("  rm X         - delete file X\n");
    print("  nano X       - edit file X\n");
    print("  cpe X        - run .cpe script\n");
    print("  run X        - run .cai application (v1.0)\n");
    print("  heap         - heap statistics\n");
    print("  reboot       - reboot\n");
}

void cmd_about() {
    print("Colibri OS - hobby OS by Asde LLC\n");
    print("Written in C, running in 32-bit mode\n");
}

void cmd_ver() {
    print("Colibri OS v0.7c (directories)\n");
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
    if (strcmp(cmd_buffer, "pwd") == 0) { fs_pwd(); return; }
    if (strcmp(cmd_buffer, "reboot") == 0) { cmd_reboot(); return; }
    if (strcmp(cmd_buffer, "heap") == 0) { heap_stats(); return; }

    if (strncmp(cmd_buffer, "echo ", 5) == 0) { cmd_echo(cmd_buffer + 5); return; }

    if (strncmp(cmd_buffer, "mkdir ", 6) == 0) {
        const char* name = cmd_buffer + 6;
        if (name[0] == 0) { print("Usage: mkdir <name>\n"); return; }
        if (fs_create(name, OBJ_DIR) != -1) {
            print("Directory created: "); print(name); putchar('\n');
        } else print("Failed (exists or no space)\n");
        return;
    }

    if (strncmp(cmd_buffer, "rmdir ", 6) == 0) {
        const char* name = cmd_buffer + 6;
        int idx = fs_find_in(current_dir, name);
        if (idx == -1 || fs_objects[idx].type != OBJ_DIR) {
            print("Directory not found\n"); return;
        }
        if (fs_delete(name)) { print("Directory removed: "); print(name); putchar('\n'); }
        else print("Failed\n");
        return;
    }

    if (strncmp(cmd_buffer, "cd ", 3) == 0) {
        const char* name = cmd_buffer + 3;
        if (name[0] == 0) { print("Usage: cd <name>\n"); return; }
        if (!fs_cd(name)) print("Directory not found\n");
        return;
    }

    if (strncmp(cmd_buffer, "touch ", 6) == 0) {
        const char* name = cmd_buffer + 6;
        if (name[0] == 0) { print("Usage: touch <name>\n"); return; }
        if (fs_create(name, OBJ_FILE) != -1) {
            print("File created: "); print(name); putchar('\n');
        } else print("Failed (exists or no space)\n");
        return;
    }

    if (strncmp(cmd_buffer, "rm ", 3) == 0) {
        const char* name = cmd_buffer + 3;
        if (name[0] == 0) { print("Usage: rm <name>\n"); return; }
        if (fs_delete(name)) { print("File deleted: "); print(name); putchar('\n'); }
        else print("File not found\n");
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
        print_color("colibri:", LIGHT_GREEN);
        /* Показываем текущий путь в приглашении */
        {
            int stack[FS_MAX_OBJECTS];
            int top = 0;
            int cur = current_dir;
            while (cur != ROOT_INDEX) {
                stack[top++] = cur;
                cur = fs_objects[cur].parent;
            }
            putchar('/');
            while (top > 0) {
                int idx = stack[--top];
                print(fs_objects[idx].name);
                if (top > 0) putchar('/');
            }
        }
        print_color("> ", LIGHT_GREEN);
        shell_read_line();
        parse_command();
    }
}

void print_banner() {
    print_color("Colibri OS v0.7c\n", LIGHT_CYAN);
    print_color("=================\n", LIGHT_CYAN);
    print("Directories + nano + cpe ready.\n");
    print("Type 'help' for commands.\n\n");
}

void kernel_main() {
    clear_screen();
    heap_init();
    fs_init();
    print_banner();
    shell_loop();
}