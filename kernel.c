/* ============================================
   Colibri OS - Ядро v0.8x (FULL)
   VGA + Shell + FS + Math + Process + Net + Env + Alias + Scroll
   Все команды из help. Подключён utils.h.
   ============================================ */

#include "kmalloc.h"
#include "utils.h"

/* ============================================
   VGA
   ============================================ */
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define BLACK         0
#define WHITE         15
#define LIGHT_GREEN   10
#define LIGHT_CYAN    11
#define LIGHT_YELLOW  14
#define LIGHT_RED     12
#define LIGHT_BLUE    9
#define LIGHT_MAGENTA 13

static unsigned char color = (BLACK << 4) | WHITE;

/* ============================================
   Scroll buffer
   ============================================ */
#define SCROLL_LINES 500
#define SCROLL_COLS  80
#define VIEW_LINES   25

static unsigned char scroll_buf[SCROLL_LINES][SCROLL_COLS];
static unsigned char scroll_col[SCROLL_LINES][SCROLL_COLS];
static int scroll_total = 0;
static int scroll_offset = 0;
static int sb_line = 0;
static int sb_col  = 0;

void scroll_redraw() {
    unsigned char* vga = (unsigned char*) VGA_MEMORY;
    int start = scroll_total - VIEW_LINES - scroll_offset;
    if (start < 0) start = 0;

    for (int i = 0; i < VIEW_LINES; i++) {
        int src = start + i;
        for (int j = 0; j < SCROLL_COLS; j++) {
            char c = (src >= 0 && src < scroll_total) ? scroll_buf[src][j] : ' ';
            unsigned char col = (src >= 0 && src < scroll_total) ? scroll_col[src][j] : color;
            int offset = (i * SCROLL_COLS + j) * 2;
            vga[offset] = c;
            vga[offset + 1] = col;
        }
    }
}

void scroll_buffer_putchar(char c) {
    if (c == '\n') {
        sb_line++;
        sb_col = 0;
        if (sb_line >= SCROLL_LINES) {
            for (int i = 0; i < SCROLL_LINES - 1; i++)
                for (int j = 0; j < SCROLL_COLS; j++) {
                    scroll_buf[i][j] = scroll_buf[i+1][j];
                    scroll_col[i][j] = scroll_col[i+1][j];
                }
            sb_line = SCROLL_LINES - 1;
            for (int j = 0; j < SCROLL_COLS; j++) {
                scroll_buf[sb_line][j] = ' ';
                scroll_col[sb_line][j] = color;
            }
        }
        if (sb_line + 1 > scroll_total) {
            scroll_total = sb_line + 1;
            if (scroll_total > SCROLL_LINES) scroll_total = SCROLL_LINES;
        }
        return;
    }
    if (c == '\b') {
        if (sb_col > 0) {
            sb_col--;
            scroll_buf[sb_line][sb_col] = ' ';
            scroll_col[sb_line][sb_col] = color;
        }
        return;
    }
    if ((unsigned char)c < 32) return;
    if (sb_col >= SCROLL_COLS) {
        sb_col = 0;
        sb_line++;
        if (sb_line >= SCROLL_LINES) {
            for (int i = 0; i < SCROLL_LINES - 1; i++)
                for (int j = 0; j < SCROLL_COLS; j++) {
                    scroll_buf[i][j] = scroll_buf[i+1][j];
                    scroll_col[i][j] = scroll_col[i+1][j];
                }
            sb_line = SCROLL_LINES - 1;
        }
    }
    scroll_buf[sb_line][sb_col] = c;
    scroll_col[sb_line][sb_col] = color;
    sb_col++;

    /* Автоскролл: если пользователь не листал вверх — окно двигается вниз */
    if (sb_line + 1 > scroll_total) {
        scroll_total = sb_line + 1;
        if (scroll_total > SCROLL_LINES) scroll_total = SCROLL_LINES;
    }
}

void clear_screen() {
    for (int i = 0; i < SCROLL_LINES; i++)
        for (int j = 0; j < SCROLL_COLS; j++) {
            scroll_buf[i][j] = ' ';
            scroll_col[i][j] = color;
        }
    scroll_total = 0;
    scroll_offset = 0;
    sb_line = 0;
    sb_col = 0;
    scroll_redraw();
}

void putchar(char c) {
    scroll_buffer_putchar(c);
    if (scroll_offset == 0) scroll_redraw();
}

void print(const char* str) { while (*str) putchar(*str++); }

void print_color(const char* str, unsigned char col) {
    unsigned char old = color;
    color = (BLACK << 4) | col;
    print(str);
    color = old;
}

static void print_dec(unsigned int n) {
    if (n == 0) { putchar('0'); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) putchar(buf[--i]);
}

static void print_hex32(unsigned int n) {
    char buf[11]; buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        int d = (n >> (28 - i * 4)) & 0xF;
        buf[2 + i] = d < 10 ? '0' + d : 'a' + d - 10;
    }
    buf[10] = 0;
    print(buf);
}

/* ============================================
   Banner
   ============================================ */
void draw_banner() {
    print_color("   ____      _ _ _          \n", LIGHT_CYAN);
    print_color("  / ___|___ | (_) |__  _ __(_)\n", LIGHT_CYAN);
    print_color(" | |   / _ \\| | | '_ \\| '__| |\n", LIGHT_CYAN);
    print_color(" | |__| (_) | | | |_) | |  | |\n", LIGHT_CYAN);
    print_color("  \\____\\___/|_|_|_.__/|_|  |_|\n", LIGHT_CYAN);
    print("\n");
    print_color("       Colibri OS v0.8x\n", LIGHT_GREEN);
    print("");
    print("Type ");
    print_color("'help'", LIGHT_YELLOW);
    print(" for commands.\n");
    print("Scroll: ");
    print_color("Arrow Up / Down", LIGHT_YELLOW);
    print("\n");
}

/* ============================================
   Keyboard
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
static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int extended = 0;

#define KEY_CTRL_C 3
#define KEY_CTRL_L 4
#define KEY_CTRL_D 5
#define KEY_ESC    27
#define KEY_UP     (-1)
#define KEY_DOWN   (-2)
#define KEY_LEFT   (-3)
#define KEY_RIGHT  (-4)
#define KEY_TAB    (-5)

char get_key() {
    while (1) {
        if (inb(0x64) & 0x01) {
            unsigned char sc = inb(0x60);
            if (sc == 0xE0) { extended = 1; continue; }
            if (sc & 0x80) {
                unsigned char r = sc & 0x7F;
                if (r == 0x2A || r == 0x36) shift_pressed = 0;
                if (r == 0x1D) ctrl_pressed = 0;
                extended = 0;
                continue;
            }
            if (extended) {
                extended = 0;
                if (sc == 0x48) return KEY_UP;
                if (sc == 0x50) return KEY_DOWN;
                if (sc == 0x4B) return KEY_LEFT;
                if (sc == 0x4D) return KEY_RIGHT;
                continue;
            }
            if (sc == 0x0F) return KEY_TAB;
            if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; continue; }
            if (sc == 0x1D) { ctrl_pressed = 1; continue; }
            if (sc < 128) {
                char c = shift_pressed ? kbd_upper[sc] : kbd_lower[sc];
                if (ctrl_pressed) {
                    if (c == 'c' || c == 'C') return KEY_CTRL_C;
                    if (c == 'l' || c == 'L') return KEY_CTRL_L;
                    if (c == 'd' || c == 'D') return KEY_CTRL_D;
                }
                if (c) return c;
            }
        }
    }
}

/*static char try_get_key() {
    if (!(inb(0x64) & 0x01)) return 0;
    unsigned char sc = inb(0x60);
    if (sc == 0xE0) { extended = 1; return 0; }
    if (sc & 0x80) {
        unsigned char r = sc & 0x7F;
        if (r == 0x2A || r == 0x36) shift_pressed = 0;
        if (r == 0x1D) ctrl_pressed = 0;
        extended = 0;
        return 0;
    }
    if (extended) {
        extended = 0;
        if (sc == 0x48) return KEY_UP;
        if (sc == 0x50) return KEY_DOWN;
        if (sc == 0x4B) return KEY_LEFT;
        if (sc == 0x4D) return KEY_RIGHT;
        return 0;
    }
    if (sc == 0x0F) return KEY_TAB;
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 0; }
    if (sc == 0x1D) { ctrl_pressed = 1; return 0; }
    if (sc < 128) {
        char c = shift_pressed ? kbd_upper[sc] : kbd_lower[sc];
        if (ctrl_pressed) {
            if (c == 'c' || c == 'C') return KEY_CTRL_C;
            if (c == 'l' || c == 'L') return KEY_CTRL_L;
            if (c == 'd' || c == 'D') return KEY_CTRL_D;
        }
        return c;
    }
    return 0;
} */

static void handle_scroll_key(char c) {
    if (c == KEY_UP) {
        if (scroll_offset < scroll_total - VIEW_LINES) {
            scroll_offset++;
            scroll_redraw();
        }
    } else if (c == KEY_DOWN) {
        if (scroll_offset > 0) {
            scroll_offset--;
            scroll_redraw();
        }
    }
}

//void pause_with_scroll() {
//    while (1) {
//        char c = try_get_key();
//        if (c == KEY_UP || c == KEY_DOWN) {
//            handle_scroll_key(c);
//            continue;
//        }
//        if (c != 0) return;
//    }
//}

/* ============================================
   Env
   ============================================ */
#define ENV_MAX 16
typedef struct { char key[32]; char val[64]; } EnvVar;
static EnvVar env_vars[ENV_MAX];
static int env_count = 0;

void env_set(const char* k, const char* v) {
    for (int i = 0; i < env_count; i++)
        if (strcmp(env_vars[i].key, k) == 0) { strncpy(env_vars[i].val, v, 63); return; }
    if (env_count < ENV_MAX) {
        strncpy(env_vars[env_count].key, k, 31);
        strncpy(env_vars[env_count].val, v, 63);
        env_count++;
    }
}

void env_unset(const char* k) {
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].key, k) == 0) {
            for (int j = i; j < env_count - 1; j++) env_vars[j] = env_vars[j+1];
            env_count--;
            return;
        }
    }
}

/* ============================================
   Alias
   ============================================ */
#define ALIAS_MAX 16
typedef struct { char name[16]; char cmd[128]; } Alias;
static Alias aliases[ALIAS_MAX];
static int alias_count = 0;

const char* alias_get(const char* name) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, name) == 0) return aliases[i].cmd;
    return 0;
}

void alias_set(const char* name, const char* cmd) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, name) == 0) { strncpy(aliases[i].cmd, cmd, 127); return; }
    if (alias_count < ALIAS_MAX) {
        strncpy(aliases[alias_count].name, name, 15);
        strncpy(aliases[alias_count].cmd, cmd, 127);
        alias_count++;
    }
}

void alias_unset(const char* name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            for (int j = i; j < alias_count - 1; j++) aliases[j] = aliases[j+1];
            alias_count--;
            return;
        }
    }
}

/* ============================================
   Filesystem
   ============================================ */
#define FS_MAX_OBJECTS 32
#define FS_NAME_LEN    32
#define FS_DATA_LEN    4096

#define OBJ_FREE 0
#define OBJ_FILE 1
#define OBJ_DIR  2
#define ROOT_INDEX 0

typedef struct {
    char name[FS_NAME_LEN];
    char data[FS_DATA_LEN];
    int  type;
    int  parent;
    int  size;
} FsObject;

static FsObject fs_objects[FS_MAX_OBJECTS];
static int current_dir = ROOT_INDEX;

int fs_find_in(int parent, const char* name) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++)
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == parent &&
            strcmp(fs_objects[i].name, name) == 0) return i;
    return -1;
}

void fs_init() {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        fs_objects[i].type = OBJ_FREE;
        fs_objects[i].name[0] = 0;
        fs_objects[i].parent = -1;
        fs_objects[i].size = 0;
        fs_objects[i].data[0] = 0;
    }
    fs_objects[ROOT_INDEX].type = OBJ_DIR;
    strcpy(fs_objects[ROOT_INDEX].name, "/");
    fs_objects[ROOT_INDEX].parent = -1;

    int c = 1; strcpy(fs_objects[c].name, "colibri"); fs_objects[c].type = OBJ_DIR; fs_objects[c].parent = ROOT_INDEX;
    int u = 2; strcpy(fs_objects[u].name, "users");   fs_objects[u].type = OBJ_DIR; fs_objects[u].parent = c;
    int a = 3; strcpy(fs_objects[a].name, "alpha");   fs_objects[a].type = OBJ_DIR; fs_objects[a].parent = u;
    int s = 4; strcpy(fs_objects[s].name, "system");  fs_objects[s].type = OBJ_DIR; fs_objects[s].parent = c;

    current_dir = a;
}

int fs_create_in(int parent_idx, const char* name, int type) {
    if (fs_find_in(parent_idx, name) != -1) return -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type == OBJ_FREE) {
            strncpy(fs_objects[i].name, name, FS_NAME_LEN - 1);
            fs_objects[i].type = type;
            fs_objects[i].parent = parent_idx;
            fs_objects[i].size = 0;
            fs_objects[i].data[0] = 0;
            return i;
        }
    }
    return -1;
}

void fs_delete_by_index(int idx) {
    if (idx == ROOT_INDEX) return;
    if (fs_objects[idx].type == OBJ_DIR)
        for (int i = 0; i < FS_MAX_OBJECTS; i++)
            if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == idx)
                fs_delete_by_index(i);
    fs_objects[idx].type = OBJ_FREE;
    fs_objects[idx].name[0] = 0;
    fs_objects[idx].parent = -1;
}

void strip_quotes(const char* in, char* out) {
    int i = 0, j = 0;
    while (in[i] == ' ' || in[i] == '\t') i++;
    if (in[i] == '"') {
        i++;
        while (in[i] && in[i] != '"' && j < 255) out[j++] = in[i++];
    } else {
        while (in[i] && in[i] != ' ' && in[i] != '\t' && j < 255) out[j++] = in[i++];
    }
    out[j] = 0;
}

int parse_path(const char* path, int start_idx) {
    char clean[256];
    strip_quotes(path, clean);
    int cur;
    const char* p = clean;
    if (clean[0] == '/') { cur = ROOT_INDEX; p = clean + 1; }
    else cur = start_idx;
    if (*p == 0) return cur;
    char part[FS_NAME_LEN];
    int pi = 0;
    while (1) {
        if (*p == '/' || *p == 0) {
            part[pi] = 0;
            if (pi > 0) {
                if (strcmp(part, ".") == 0) { }
                else if (strcmp(part, "..") == 0) { if (cur != ROOT_INDEX) cur = fs_objects[cur].parent; }
                else { int idx = fs_find_in(cur, part); if (idx == -1) return -1; cur = idx; }
            }
            if (*p == 0) break;
            pi = 0; p++;
            continue;
        }
        if (pi < FS_NAME_LEN - 1) part[pi++] = *p;
        p++;
    }
    return cur;
}

int split_path(const char* path, int start_idx, int* parent_idx, char* last) {
    char clean[256];
    strip_quotes(path, clean);
    int cur;
    const char* p = clean;
    if (clean[0] == '/') { cur = ROOT_INDEX; p = clean + 1; }
    else cur = start_idx;
    char part[FS_NAME_LEN];
    int pi = 0;
    int last_parent = cur;
    char last_name[FS_NAME_LEN];
    last_name[0] = 0;
    while (1) {
        if (*p == '/' || *p == 0) {
            part[pi] = 0;
            if (pi > 0) {
                if (last_name[0] != 0) {
                    if (strcmp(last_name, ".") == 0) { }
                    else if (strcmp(last_name, "..") == 0) { if (last_parent != ROOT_INDEX) last_parent = fs_objects[last_parent].parent; }
                    else { int idx = fs_find_in(last_parent, last_name); if (idx == -1) return 0; last_parent = idx; }
                }
                strcpy(last_name, part);
            }
            if (*p == 0) break;
            pi = 0; p++;
            continue;
        }
        if (pi < FS_NAME_LEN - 1) part[pi++] = *p;
        p++;
    }
    *parent_idx = last_parent;
    strncpy(last, last_name, FS_NAME_LEN - 1);
    return (last_name[0] != 0);
}

/* ============================================
   Recursive
   ============================================ */
void tree_recursive(int dir_idx, int depth) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == dir_idx) {
            for (int j = 0; j < depth; j++) print("  ");
            if (fs_objects[i].type == OBJ_DIR) {
                print_color("[D] ", LIGHT_CYAN);
                print_color(fs_objects[i].name, LIGHT_CYAN);
                putchar('\n');
                tree_recursive(i, depth + 1);
            } else {
                print("[F] ");
                print(fs_objects[i].name);
                putchar('\n');
            }
        }
    }
}

void find_recursive(int dir_idx, const char* name, int* found) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == dir_idx) {
            if (strcmp(fs_objects[i].name, name) == 0) {
                print("  ");
                if (fs_objects[i].type == OBJ_DIR) print_color("[D] ", LIGHT_CYAN);
                else print("[F] ");
                print(fs_objects[i].name);
                putchar('\n');
                *found = 1;
            }
            if (fs_objects[i].type == OBJ_DIR) find_recursive(i, name, found);
        }
    }
}

/* ============================================
   Calculator
   ============================================ */
static int calc_parse_primary(const char** p, int* ok);
static int calc_parse_unary(const char** p, int* ok);
static int calc_parse_power(const char** p, int* ok);
static int calc_parse_term(const char** p, int* ok);
static int calc_parse_expr(const char** p, int* ok);

static void calc_skip_spaces(const char** p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

static int calc_parse_primary(const char** p, int* ok) {
    calc_skip_spaces(p);
    if (!**p) { *ok = 0; return 0; }

    if (**p == '(') {
        (*p)++;
        int value = calc_parse_expr(p, ok);
        if (!*ok) return 0;
        calc_skip_spaces(p);
        if (**p != ')') { *ok = 0; return 0; }
        (*p)++;
        return value;
    }

    if (**p == '-' || **p == '+') {
        char sign = **p;
        (*p)++;
        int value = calc_parse_primary(p, ok);
        if (!*ok) return 0;
        return sign == '-' ? -value : value;
    }

    if (*(*p) >= '0' && *(*p) <= '9') {
        int value = 0;
        while (*(*p) >= '0' && *(*p) <= '9') {
            value = value * 10 + (*(*p) - '0');
            (*p)++;
        }
        return value;
    }

    *ok = 0;
    return 0;
}

static int calc_parse_power(const char** p, int* ok) {
    int left = calc_parse_primary(p, ok);
    if (!*ok) return 0;

    calc_skip_spaces(p);
    if (**p == '^') {
        (*p)++;
        int right = calc_parse_power(p, ok);
        if (!*ok) return 0;
        return pow_int(left, right);
    }

    return left;
}

static int calc_parse_term(const char** p, int* ok) {
    int value = calc_parse_power(p, ok);
    if (!*ok) return 0;

    while (1) {
        calc_skip_spaces(p);
        if (!**p) break;

        char op = **p;
        if (op != '*' && op != '/' && op != '%') break;
        (*p)++;

        int rhs = calc_parse_power(p, ok);
        if (!*ok) return 0;

        if (op == '*') value *= rhs;
        else if (op == '/') {
            if (rhs == 0) { *ok = 0; return 0; }
            value /= rhs;
        } else {
            if (rhs == 0) { *ok = 0; return 0; }
            value %= rhs;
        }
    }

    return value;
}

static int calc_parse_expr(const char** p, int* ok) {
    int value = calc_parse_term(p, ok);
    if (!*ok) return 0;

    while (1) {
        calc_skip_spaces(p);
        if (!**p) break;

        char op = **p;
        if (op != '+' && op != '-') break;
        (*p)++;

        int rhs = calc_parse_term(p, ok);
        if (!*ok) return 0;

        if (op == '+') value += rhs;
        else value -= rhs;
    }

    return value;
}

static int calc_expr(const char* expr, int* ok) {
    const char* p = expr;
    *ok = 1;
    calc_skip_spaces(&p);
    int value = calc_parse_expr(&p, ok);
    if (!*ok) return 0;
    calc_skip_spaces(&p);
    if (*p != 0) { *ok = 0; return 0; }
    return value;
}

/* ============================================
   Processes (stub)
   ============================================ */
#define PROC_MAX 8
typedef struct { int pid; int active; char name[32]; } Proc;
static Proc procs[PROC_MAX];
static int next_pid = 1;

void proc_init() { for (int i = 0; i < PROC_MAX; i++) procs[i].active = 0; }

int proc_spawn(const char* name) {
    for (int i = 0; i < PROC_MAX; i++)
        if (!procs[i].active) {
            procs[i].active = 1;
            procs[i].pid = next_pid++;
            strncpy(procs[i].name, name, 31);
            return procs[i].pid;
        }
    return -1;
}

int proc_kill(int pid) {
    for (int i = 0; i < PROC_MAX; i++)
        if (procs[i].active && procs[i].pid == pid) { procs[i].active = 0; return 1; }
    return 0;
}

/* ============================================
   Net (stub)
   ============================================ */
static int net_up = 0;
static const char* net_ip   = "10.0.2.15";
static const char* net_mask = "255.255.255.0";
static const char* net_gw   = "10.0.2.2";

/* ============================================
   Forward
   ============================================ */

/* ============================================
   System commands
   ============================================ */
void cmd_help() {
    print_color("=== Colibri OS v0.8x - Commands ===\n", LIGHT_CYAN);
    print_color("--- System ---\n", LIGHT_YELLOW);
    print("  help          - this help\n");
    print("  ver           - version\n");
    print("  banner        - show banner\n");
    print("  clear         - clear screen\n");
    print("  date          - date & time\n");
    print("  uptime        - time since boot\n");
    print("  mem           - memory info\n");
    print("  heap          - heap stats\n");
    print("  history       - last commands\n");
    print("  color <n>     - text color (0-15)\n");
    print("  reboot        - restart\n");
    print("  shutdown      - halt CPU\n");
    print_color("--- Files ---\n", LIGHT_YELLOW);
    print("  ls [path]     - list dir\n");
    print("  pwd           - current dir\n");
    print("  cd <path>     - change dir\n");
    print("  mkdir X       - make dir\n");
    print("  rmdir X       - remove dir\n");
    print("  touch X       - create file\n");
    print("  rm X          - delete\n");
    print("  cat <file>    - show file\n");
    print("  echo X > Y    - write X to Y\n");
    print("  echo X >> Y   - append X to Y\n");
    print("  write <f> <t> - write text\n");
    print("  append <f> <t>- append text\n");
    print("  trunc <file>  - clear file\n");
    print("  cp <src> <dst>- copy file\n");
    print("  mv <src> <dst>- rename\n");
    print("  tree          - show tree\n");
    print("  find <name>   - find file\n");
    print("  stat <file>   - file info\n");
    print("  wc <file>     - lines/words/chars\n");
    print("  head <file>   - first 10 lines\n");
    print("  tail <file>   - last 10 lines\n");
    print("  grep <pat> <f>- search in file\n");
    print("  sort <file>   - sort lines\n");
    print("  hexdump <file>- hex dump\n");
    print_color("--- Math ---\n", LIGHT_YELLOW);
    print("  calc <expr>   - + - * / %% ^\n");
    print("  sqrt <n>      - square root\n");
    print("  pow <a> <b>   - a^b\n");
    print("  rand          - random 0-99\n");
    print("  prime <n>     - is prime?\n");
    print("  gcd <a> <b>   - GCD\n");
    print("  lcm <a> <b>   - LCM\n");
    print("  hex <n>       - decimal to hex\n");
    print("  bin <n>       - decimal to binary\n");
    print_color("--- Misc ---\n", LIGHT_YELLOW);
    print("  echo X        - print X\n");
    print("  sleep <ms>    - pause\n");
    print("  beep          - PC speaker beep\n");
    print_color("--- Process (stub) ---\n", LIGHT_YELLOW);
    print("  ps            - list processes\n");
    print("  spawn <name>  - spawn process\n");
    print("  kill <pid>    - kill process\n");
    print_color("--- Net (stub) ---\n", LIGHT_YELLOW);
    print("  ifconfig      - show interfaces\n");
    print("  ping <host>   - ping host\n");
    print_color("--- Env / Alias ---\n", LIGHT_YELLOW);
    print("  env           - show vars\n");
    print("  setenv K=V    - set var\n");
    print("  unsetenv K    - unset var\n");
    print("  alias         - list aliases\n");
    print("  alias X=Y     - set alias\n");
    print("  unalias X     - remove alias\n");
    print("\n");
}

void cmd_ver() { print("Colibri OS v0.8x (full, with utils)\n"); }

void cmd_pwd_no_newline() {
    int stack[FS_MAX_OBJECTS];
    int top = 0, cur = current_dir;
    while (cur != ROOT_INDEX) { stack[top++] = cur; cur = fs_objects[cur].parent; }
    print("/");
    while (top > 0) { int idx = stack[--top]; print(fs_objects[idx].name); if (top > 0) print("/"); }
}

void cmd_pwd() { cmd_pwd_no_newline(); putchar('\n'); }

void cmd_date() {
    rtc_time_t t = rtc_get_time();
    print_dec(t.day); putchar('.');
    if (t.mon < 10) putchar('0'); print_dec(t.mon); putchar('.');
    print_dec(t.year); putchar(' ');
    if (t.hour < 10) putchar('0'); print_dec(t.hour); putchar(':');
    if (t.min < 10) putchar('0'); print_dec(t.min); putchar(':');
    if (t.sec < 10) putchar('0'); print_dec(t.sec);
    putchar('\n');
}

void cmd_uptime() {
    rtc_time_t t = rtc_get_time();
    print("Uptime: current time ");
    print_dec(t.hour); putchar(':');
    if (t.min < 10) putchar('0'); print_dec(t.min); putchar(':');
    if (t.sec < 10) putchar('0'); print_dec(t.sec);
    putchar('\n');
}

void cmd_mem() {
    print("Heap start: "); print_hex32(0x100000);
    print("\nHeap size:  1048576 bytes\n");
    print("VGA:        text mode 80x25 @ 0xB8000\n");
}

void cmd_color(const char* arg) {
    int n = atoi(arg);
    if (n < 0 || n > 15) { print("0-15\n"); return; }
    color = (BLACK << 4) | (unsigned char)n;
}

void cmd_sleep(const char* arg) {
    int ms = atoi(arg);
    if (ms < 0) ms = 0;
    for (volatile int i = 0; i < ms * 1000; i++) { }
    print("Slept ");
    print_dec(ms);
    print(" ms\n");
}

void cmd_beep() {
    outb(0x43, 0xB6);
    unsigned int div = 1193180 / 440;
    outb(0x42, (unsigned char)(div & 0xFF));
    outb(0x42, (unsigned char)((div >> 8) & 0xFF));
    outb(0x61, inb(0x61) | 3);
    for (volatile int i = 0; i < 2000000; i++) { }
    outb(0x61, inb(0x61) & ~3);
    print("Beep!\n");
}

void cmd_reboot() {
    print("Rebooting...\n");
    while (inb(0x64) & 0x02) { }
    outb(0x64, 0xFE);
    cpu_cli();
    while (1) cpu_halt();
}

void cmd_shutdown() {
    print("Shutting down...\n");
    outb(0x604, 0x00);
    outb(0x604, 0x20);
    outb(0xB004, 0x00);
    outb(0xB004, 0x20);
    cpu_cli();
    while (1) cpu_halt();
}

/* ============================================
   File commands
   ============================================ */
void cmd_ls(const char* path) {
    int dir = current_dir;
    if (path && path[0] != 0) {
        int idx = parse_path(path, current_dir);
        if (idx == -1 || fs_objects[idx].type != OBJ_DIR) { print("Not a directory\n"); return; }
        dir = idx;
    }
    int count = 0;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == dir) {
            if (fs_objects[i].type == OBJ_DIR) {
                print_color("  ", LIGHT_BLUE);
                print_color(fs_objects[i].name, LIGHT_BLUE);
                print("/\n");
            } else {
                print("  "); print(fs_objects[i].name);
                print("  ("); print_dec(fs_objects[i].size); print(" bytes)\n");
            }
            count++;
        }
    }
    if (count == 0) print("  (empty)\n");
}

void cmd_cat(const char* path) {
    int idx = parse_path(path, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    print(fs_objects[idx].data);
    if (fs_objects[idx].size > 0 && fs_objects[idx].data[fs_objects[idx].size - 1] != '\n') putchar('\n');
}

void cmd_echo_redirect(const char* text, const char* filename, int append) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(filename, current_dir, &parent, last)) { print("Invalid path\n"); return; }
    int idx = fs_find_in(parent, last);
    if (idx == -1) {
        idx = fs_create_in(parent, last, OBJ_FILE);
        if (idx == -1) { print("Cannot create file\n"); return; }
    }
    int tlen = strlen(text);
    int base = append ? fs_objects[idx].size : 0;
    if (base + tlen > FS_DATA_LEN - 2) tlen = FS_DATA_LEN - 2 - base;
    strncpy(fs_objects[idx].data + base, text, tlen);
    fs_objects[idx].data[base + tlen] = '\n';
    fs_objects[idx].size = base + tlen + 1;
    print(append ? "Appended " : "Written ");
    print_dec(tlen + 1); print(" bytes to "); print(filename); putchar('\n');
}

void cmd_cp(const char* src, const char* dst) {
    int sidx = parse_path(src, current_dir);
    if (sidx == -1 || fs_objects[sidx].type != OBJ_FILE) { print("Source not found\n"); return; }
    int parent; char last[FS_NAME_LEN];
    if (!split_path(dst, current_dir, &parent, last)) { print("Invalid dst\n"); return; }
    int didx = fs_find_in(parent, last);
    if (didx == -1) {
        didx = fs_create_in(parent, last, OBJ_FILE);
        if (didx == -1) { print("Cannot create dst\n"); return; }
    }
    strncpy(fs_objects[didx].data, fs_objects[sidx].data, FS_DATA_LEN);
    fs_objects[didx].size = fs_objects[sidx].size;
    print("Copied.\n");
}

void cmd_mv(const char* src, const char* dst) {
    int sidx = parse_path(src, current_dir);
    if (sidx == -1) { print("Source not found\n"); return; }
    int parent; char last[FS_NAME_LEN];
    if (!split_path(dst, current_dir, &parent, last)) { print("Invalid dst\n"); return; }
    if (fs_find_in(parent, last) != -1) { print("Dst exists\n"); return; }
    strncpy(fs_objects[sidx].name, last, FS_NAME_LEN - 1);
    fs_objects[sidx].parent = parent;
    print("Moved.\n");
}

void cmd_touch(const char* name) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(name, current_dir, &parent, last)) { print("Invalid name\n"); return; }
    if (fs_find_in(parent, last) != -1) { print("Exists\n"); return; }
    if (fs_create_in(parent, last, OBJ_FILE) == -1) print("Cannot create\n");
    else print("Created.\n");
}

void cmd_mkdir(const char* name) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(name, current_dir, &parent, last)) { print("Invalid name\n"); return; }
    if (fs_find_in(parent, last) != -1) { print("Exists\n"); return; }
    if (fs_create_in(parent, last, OBJ_DIR) == -1) print("Cannot create\n");
    else print("Created dir.\n");
}

void cmd_rm(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1) { print("Not found\n"); return; }
    if (idx == ROOT_INDEX) { print("Cannot remove root\n"); return; }
    if (idx == current_dir) { print("Cannot remove current dir\n"); return; }
    fs_delete_by_index(idx);
    print("Removed.\n");
}

void cmd_rmdir(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1) { print("Not found\n"); return; }
    if (fs_objects[idx].type != OBJ_DIR) { print("Not a dir\n"); return; }
    if (idx == current_dir) { print("Cannot remove current dir\n"); return; }
    fs_delete_by_index(idx);
    print("Removed dir.\n");
}

void cmd_cd(const char* path) {
    int idx = parse_path(path, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_DIR) { print("Not a directory\n"); return; }
    current_dir = idx;
}

void cmd_tree() { tree_recursive(current_dir, 0); }

void cmd_find(const char* name) {
    int found = 0;
    find_recursive(current_dir, name, &found);
    if (!found) print("Not found\n");
}

void cmd_stat(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1) { print("Not found\n"); return; }
    print("Name:   "); print(fs_objects[idx].name); putchar('\n');
    print("Type:   "); print(fs_objects[idx].type == OBJ_DIR ? "dir\n" : "file\n");
    print("Size:   "); print_dec(fs_objects[idx].size); print(" bytes\n");
    print("Parent: "); print_dec(fs_objects[idx].parent); putchar('\n');
}

void cmd_wc(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    int lines = 0, words = 0, chars = fs_objects[idx].size;
    int in_word = 0;
    for (int i = 0; i < fs_objects[idx].size; i++) {
        char c = fs_objects[idx].data[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n') in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    print_dec(lines); print(" lines, ");
    print_dec(words); print(" words, ");
    print_dec(chars); print(" bytes\n");
}

void cmd_head(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    int lines = 0;
    for (int i = 0; i < fs_objects[idx].size && lines < 10; i++) {
        putchar(fs_objects[idx].data[i]);
        if (fs_objects[idx].data[i] == '\n') lines++;
    }
    if (lines == 0 || fs_objects[idx].data[fs_objects[idx].size-1] != '\n') putchar('\n');
}

void cmd_tail(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    int total = 0;
    for (int i = 0; i < fs_objects[idx].size; i++)
        if (fs_objects[idx].data[i] == '\n') total++;
    int start_line = total > 10 ? total - 10 : 0;
    int cur = 0;
    for (int i = 0; i < fs_objects[idx].size; i++) {
        if (cur >= start_line) putchar(fs_objects[idx].data[i]);
        if (fs_objects[idx].data[i] == '\n') cur++;
    }
    putchar('\n');
}

void cmd_grep(const char* pat, const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    char line[256];
    int li = 0;
    for (int i = 0; i <= fs_objects[idx].size; i++) {
        char c = (i < fs_objects[idx].size) ? fs_objects[idx].data[i] : '\n';
        if (c == '\n') {
            line[li] = 0;
            if (li > 0 && strstr(line, pat)) { print(line); putchar('\n'); }
            li = 0;
        } else if (li < 255) line[li++] = c;
    }
}

void cmd_sort(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    char lines[64][128];
    int lc = 0;
    int li = 0;
    for (int i = 0; i <= fs_objects[idx].size && lc < 64; i++) {
        char c = (i < fs_objects[idx].size) ? fs_objects[idx].data[i] : '\n';
        if (c == '\n') {
            lines[lc][li] = 0;
            lc++; li = 0;
        } else if (li < 127) lines[lc][li++] = c;
    }
    for (int i = 0; i < lc - 1; i++)
        for (int j = 0; j < lc - 1 - i; j++)
            if (strcmp(lines[j], lines[j+1]) > 0) {
                char tmp[128];
                strcpy(tmp, lines[j]);
                strcpy(lines[j], lines[j+1]);
                strcpy(lines[j+1], tmp);
            }
    for (int i = 0; i < lc; i++) { print(lines[i]); putchar('\n'); }
}

void cmd_hexdump(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    dump_hex(fs_objects[idx].data, fs_objects[idx].size);
}

void cmd_trunc(const char* name) {
    int idx = parse_path(name, current_dir);
    if (idx == -1 || fs_objects[idx].type != OBJ_FILE) { print("File not found\n"); return; }
    fs_objects[idx].size = 0;
    fs_objects[idx].data[0] = 0;
    print("Truncated.\n");
}

void cmd_write(const char* name, const char* text) { cmd_echo_redirect(text, name, 0); }
void cmd_append(const char* name, const char* text) { cmd_echo_redirect(text, name, 1); }

/* ============================================
   Math commands
   ============================================ */
void cmd_calc(const char* expr) {
    int ok;
    int r = calc_expr(expr, &ok);
    if (!ok) { print("Error\n"); return; }
    print("= "); print_dec(r); putchar('\n');
}

void cmd_sqrt(const char* arg) {
    int n = atoi(arg);
    print("sqrt("); print_dec(n); print(") = ");
    print_dec(sqrt_int(n)); putchar('\n');
}

void cmd_pow(const char* a, const char* b) {
    int x = atoi(a), y = atoi(b);
    print_dec(x); print("^"); print_dec(y); print(" = ");
    print_dec(pow_int(x, y)); putchar('\n');
}

void cmd_rand() { print_dec(rand_range(0, 99)); putchar('\n'); }

void cmd_prime(const char* arg) {
    int n = atoi(arg);
    if (is_prime(n)) { print_dec(n); print(" is prime\n"); }
    else { print_dec(n); print(" is not prime\n"); }
}

void cmd_gcd(const char* a, const char* b) { print_dec(gcd(atoi(a), atoi(b))); putchar('\n'); }
void cmd_lcm(const char* a, const char* b) { print_dec(lcm(atoi(a), atoi(b))); putchar('\n'); }

void cmd_hex(const char* arg) {
    int n = atoi(arg);
    char buf[16];
    itoa(n, buf, 16);
    print("0x"); print(buf); putchar('\n');
}

void cmd_bin(const char* arg) {
    int n = atoi(arg);
    char buf[40];
    itoa(n, buf, 2);
    print(buf); putchar('\n');
}

/* ============================================
   Env / alias commands
   ============================================ */
void cmd_env() {
    if (env_count == 0) { print("(no vars)\n"); return; }
    for (int i = 0; i < env_count; i++) {
        print(env_vars[i].key);
        print("=");
        print(env_vars[i].val);
        putchar('\n');
    }
}

void cmd_setenv(const char* kv) {
    char key[32], val[64];
    int i = 0, j = 0;
    while (kv[i] && kv[i] != '=' && j < 31) key[j++] = kv[i++];
    key[j] = 0;
    if (kv[i] != '=') { print("Usage: setenv K=V\n"); return; }
    i++; j = 0;
    while (kv[i] && j < 63) val[j++] = kv[i++];
    val[j] = 0;
    env_set(key, val);
    print("OK\n");
}

void cmd_unsetenv(const char* key) { env_unset(key); print("OK\n"); }

void cmd_alias_list() {
    if (alias_count == 0) { print("(no aliases)\n"); return; }
    for (int i = 0; i < alias_count; i++) {
        print(aliases[i].name);
        print(" = ");
        print(aliases[i].cmd);
        putchar('\n');
    }
}

void cmd_alias_set(const char* kv) {
    char name[16], cmd[128];
    int i = 0, j = 0;
    while (kv[i] && kv[i] != '=' && j < 15) name[j++] = kv[i++];
    name[j] = 0;
    if (kv[i] != '=') { print("Usage: alias X=Y\n"); return; }
    i++; j = 0;
    while (kv[i] && j < 127) cmd[j++] = kv[i++];
    cmd[j] = 0;
    alias_set(name, cmd);
    print("OK\n");
}

void cmd_unalias(const char* name) { alias_unset(name); print("OK\n"); }

/* ============================================
   Process commands
   ============================================ */
void cmd_ps() {
    int any = 0;
    print("PID  NAME\n");
    for (int i = 0; i < PROC_MAX; i++)
        if (procs[i].active) {
            print_dec(procs[i].pid); print("    ");
            print(procs[i].name); putchar('\n');
            any = 1;
        }
    if (!any) print("(no processes)\n");
}

void cmd_spawn(const char* name) {
    int pid = proc_spawn(name);
    if (pid < 0) { print("No slots\n"); return; }
    print("Spawned PID "); print_dec(pid); putchar('\n');
}

void cmd_kill(const char* arg) {
    int pid = atoi(arg);
    if (proc_kill(pid)) print("Killed\n");
    else print("No such process\n");
}

/* ============================================
   Net commands
   ============================================ */
void cmd_ifconfig() {
    print("eth0: ");
    print(net_up ? "UP" : "DOWN");
    putchar('\n');
    print("  ip:   "); print(net_ip); putchar('\n');
    print("  mask: "); print(net_mask); putchar('\n');
    print("  gw:   "); print(net_gw); putchar('\n');
}

void cmd_ping(const char* host) {
    print("PING ");
    print(host);
    print(" (stub): 4 packets, 0% loss\n");
}

/* ============================================
   History
   ============================================ */
#define LINE_MAX 256
#define HIST_MAX 16

static char history[HIST_MAX][LINE_MAX];
static int history_count = 0;

static void history_add(const char* line) {
    if (!line || !line[0]) return;
    if (history_count > 0 && strcmp(history[history_count-1], line) == 0) return;
    if (history_count < HIST_MAX) {
        strncpy(history[history_count], line, LINE_MAX - 1);
        history[history_count][LINE_MAX-1] = 0;
        history_count++;
    } else {
        for (int i = 0; i < HIST_MAX - 1; i++) strcpy(history[i], history[i+1]);
        strncpy(history[HIST_MAX-1], line, LINE_MAX - 1);
        history[HIST_MAX-1][LINE_MAX-1] = 0;
    }
}

void cmd_history() {
    for (int i = 0; i < history_count; i++) {
        print_dec(i + 1);
        print("  ");
        print(history[i]);
        putchar('\n');
    }
}

/* ============================================
   Parser
   ============================================ */
static int tokenize(char* line, char tokens[][128], int max_tokens) {
    int n = 0;
    char* p = line;
    while (*p && n < max_tokens) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < 127) tokens[n][i++] = *p++;
        tokens[n][i] = 0;
        n++;
    }
    return n;
}

static int find_redirect(char tokens[][128], int n, int* append) {
    for (int i = 0; i < n; i++) {
        if (strcmp(tokens[i], ">") == 0) { *append = 0; return i; }
        if (strcmp(tokens[i], ">>") == 0) { *append = 1; return i; }
    }
    return -1;
}

void shell_execute(char* line) {
    trim(line);
    if (!line[0]) return;

    const char* al = alias_get(line);
    if (al) {
        char tmp[LINE_MAX];
        strncpy(tmp, al, LINE_MAX - 1);
        tmp[LINE_MAX-1] = 0;
        shell_execute(tmp);
        return;
    }

    char tokens[16][128];
    int n = tokenize(line, tokens, 16);
    if (n == 0) return;

    int append = 0;
    int redir = find_redirect(tokens, n, &append);
    if (redir >= 0) {
        char text[LINE_MAX] = {0};
        for (int i = 1; i < redir; i++) {
            if (i > 1) strncat(text, " ", LINE_MAX - strlen(text) - 1);
            strncat(text, tokens[i], LINE_MAX - strlen(text) - 1);
        }
        if (redir + 1 < n) cmd_echo_redirect(text, tokens[redir + 1], append);
        else print("Missing filename\n");
        return;
    }

    const char* cmd = tokens[0];

    /* System */
    if (strcmp(cmd, "help") == 0) cmd_help();
    else if (strcmp(cmd, "ver") == 0) cmd_ver();
    else if (strcmp(cmd, "banner") == 0) draw_banner();
    else if (strcmp(cmd, "clear") == 0) { clear_screen(); draw_banner(); }
    else if (strcmp(cmd, "date") == 0) cmd_date();
    else if (strcmp(cmd, "uptime") == 0) cmd_uptime();
    else if (strcmp(cmd, "mem") == 0) cmd_mem();
    else if (strcmp(cmd, "heap") == 0) heap_stats();
    else if (strcmp(cmd, "history") == 0) cmd_history();
    else if (strcmp(cmd, "color") == 0) { if (n > 1) cmd_color(tokens[1]); else print("Usage: color <0-15>\n"); }
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (strcmp(cmd, "shutdown") == 0) cmd_shutdown();
    /* Files */
    else if (strcmp(cmd, "ls") == 0) cmd_ls(n > 1 ? tokens[1] : "");
    else if (strcmp(cmd, "pwd") == 0) cmd_pwd();
    else if (strcmp(cmd, "cd") == 0) { if (n > 1) cmd_cd(tokens[1]); else cmd_cd("/"); }
    else if (strcmp(cmd, "mkdir") == 0) { if (n > 1) cmd_mkdir(tokens[1]); else print("Usage: mkdir X\n"); }
    else if (strcmp(cmd, "rmdir") == 0) { if (n > 1) cmd_rmdir(tokens[1]); else print("Usage: rmdir X\n"); }
    else if (strcmp(cmd, "touch") == 0) { if (n > 1) cmd_touch(tokens[1]); else print("Usage: touch X\n"); }
    else if (strcmp(cmd, "rm") == 0) { if (n > 1) cmd_rm(tokens[1]); else print("Usage: rm X\n"); }
    else if (strcmp(cmd, "cat") == 0) { if (n > 1) cmd_cat(tokens[1]); else print("Usage: cat <file>\n"); }
    else if (strcmp(cmd, "write") == 0) {
        if (n < 3) { print("Usage: write <file> <text>\n"); }
        else {
            char text[LINE_MAX] = {0};
            for (int i = 2; i < n; i++) {
                if (i > 2) strncat(text, " ", LINE_MAX - strlen(text) - 1);
                strncat(text, tokens[i], LINE_MAX - strlen(text) - 1);
            }
            cmd_write(tokens[1], text);
        }
    }
    else if (strcmp(cmd, "append") == 0) {
        if (n < 3) { print("Usage: append <file> <text>\n"); }
        else {
            char text[LINE_MAX] = {0};
            for (int i = 2; i < n; i++) {
                if (i > 2) strncat(text, " ", LINE_MAX - strlen(text) - 1);
                strncat(text, tokens[i], LINE_MAX - strlen(text) - 1);
            }
            cmd_append(tokens[1], text);
        }
    }
    else if (strcmp(cmd, "trunc") == 0) { if (n > 1) cmd_trunc(tokens[1]); else print("Usage: trunc <file>\n"); }
    else if (strcmp(cmd, "cp") == 0) { if (n > 2) cmd_cp(tokens[1], tokens[2]); else print("Usage: cp <src> <dst>\n"); }
    else if (strcmp(cmd, "mv") == 0) { if (n > 2) cmd_mv(tokens[1], tokens[2]); else print("Usage: mv <src> <dst>\n"); }
    else if (strcmp(cmd, "tree") == 0) cmd_tree();
    else if (strcmp(cmd, "find") == 0) { if (n > 1) cmd_find(tokens[1]); else print("Usage: find <name>\n"); }
    else if (strcmp(cmd, "stat") == 0) { if (n > 1) cmd_stat(tokens[1]); else print("Usage: stat <file>\n"); }
    else if (strcmp(cmd, "wc") == 0) { if (n > 1) cmd_wc(tokens[1]); else print("Usage: wc <file>\n"); }
    else if (strcmp(cmd, "head") == 0) { if (n > 1) cmd_head(tokens[1]); else print("Usage: head <file>\n"); }
    else if (strcmp(cmd, "tail") == 0) { if (n > 1) cmd_tail(tokens[1]); else print("Usage: tail <file>\n"); }
    else if (strcmp(cmd, "grep") == 0) { if (n > 2) cmd_grep(tokens[1], tokens[2]); else print("Usage: grep <pat> <file>\n"); }
    else if (strcmp(cmd, "sort") == 0) { if (n > 1) cmd_sort(tokens[1]); else print("Usage: sort <file>\n"); }
    else if (strcmp(cmd, "hexdump") == 0) { if (n > 1) cmd_hexdump(tokens[1]); else print("Usage: hexdump <file>\n"); }
    /* Math */
    else if (strcmp(cmd, "calc") == 0) {
        if (n < 2) { print("Usage: calc <expr>\n"); }
        else {
            char expr[LINE_MAX] = {0};
            for (int i = 1; i < n; i++) {
                if (i > 1) strncat(expr, " ", LINE_MAX - strlen(expr) - 1);
                strncat(expr, tokens[i], LINE_MAX - strlen(expr) - 1);
            }
            cmd_calc(expr);
        }
    }
    else if (strcmp(cmd, "sqrt") == 0) { if (n > 1) cmd_sqrt(tokens[1]); else print("Usage: sqrt <n>\n"); }
    else if (strcmp(cmd, "pow") == 0) { if (n > 2) cmd_pow(tokens[1], tokens[2]); else print("Usage: pow <a> <b>\n"); }
    else if (strcmp(cmd, "rand") == 0) cmd_rand();
    else if (strcmp(cmd, "prime") == 0) { if (n > 1) cmd_prime(tokens[1]); else print("Usage: prime <n>\n"); }
    else if (strcmp(cmd, "gcd") == 0) { if (n > 2) cmd_gcd(tokens[1], tokens[2]); else print("Usage: gcd <a> <b>\n"); }
    else if (strcmp(cmd, "lcm") == 0) { if (n > 2) cmd_lcm(tokens[1], tokens[2]); else print("Usage: lcm <a> <b>\n"); }
    else if (strcmp(cmd, "hex") == 0) { if (n > 1) cmd_hex(tokens[1]); else print("Usage: hex <n>\n"); }
    else if (strcmp(cmd, "bin") == 0) { if (n > 1) cmd_bin(tokens[1]); else print("Usage: bin <n>\n"); }
    /* Misc */
    else if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; i < n; i++) { if (i > 1) putchar(' '); print(tokens[i]); }
        putchar('\n');
    }
    else if (strcmp(cmd, "sleep") == 0) { if (n > 1) cmd_sleep(tokens[1]); else print("Usage: sleep <ms>\n"); }
    else if (strcmp(cmd, "beep") == 0) cmd_beep();
    /* Process */
    else if (strcmp(cmd, "ps") == 0) cmd_ps();
    else if (strcmp(cmd, "spawn") == 0) { if (n > 1) cmd_spawn(tokens[1]); else print("Usage: spawn <name>\n"); }
    else if (strcmp(cmd, "kill") == 0) { if (n > 1) cmd_kill(tokens[1]); else print("Usage: kill <pid>\n"); }
    /* Net */
    else if (strcmp(cmd, "ifconfig") == 0) cmd_ifconfig();
    else if (strcmp(cmd, "ping") == 0) { if (n > 1) cmd_ping(tokens[1]); else print("Usage: ping <host>\n"); }
    /* Env / Alias */
    else if (strcmp(cmd, "env") == 0) cmd_env();
    else if (strcmp(cmd, "setenv") == 0) { if (n > 1) cmd_setenv(tokens[1]); else print("Usage: setenv K=V\n"); }
    else if (strcmp(cmd, "unsetenv") == 0) { if (n > 1) cmd_unsetenv(tokens[1]); else print("Usage: unsetenv K\n"); }
    else if (strcmp(cmd, "alias") == 0) {
        if (n == 1) cmd_alias_list();
        else cmd_alias_set(tokens[1]);
    }
    else if (strcmp(cmd, "unalias") == 0) { if (n > 1) cmd_unalias(tokens[1]); else print("Usage: unalias X\n"); }
    else {
        print("Unknown command: ");
        print(cmd);
        putchar('\n');
        print("Type 'help' for list.\n");
    }
}

/* ============================================
   Shell
   ============================================ */
static char input_buf[LINE_MAX];
static int input_len = 0;

/* Прямой вывод в VGA - обходит print/scroll/putchar */
static int shell_row = 0;
static int shell_col = 0;

static void shell_puts(const char* s) {
    volatile unsigned char* vga = (unsigned char*) 0xB8000;
    while (*s) {
        if (*s == '\n') {
            shell_row++;
            shell_col = 0;
        } else {
            if (shell_col >= 80) { shell_col = 0; shell_row++; }
            int off = (shell_row * 80 + shell_col) * 2;
            vga[off] = *s;
            vga[off + 1] = 0x0F;
            shell_col++;
        }
        s++;
    }
}

static void shell_putchar(char c) {
    volatile unsigned char* vga = (unsigned char*) 0xB8000;
    if (c == '\n') {
        shell_row++;
        shell_col = 0;
    } else if (c == '\b') {
        if (shell_col > 0) {
            shell_col--;
            int off = (shell_row * 80 + shell_col) * 2;
            vga[off] = ' ';
            vga[off + 1] = 0x0F;
        }
    } else {
        if (shell_col >= 80) { shell_col = 0; shell_row++; }
        int off = (shell_row * 80 + shell_col) * 2;
        vga[off] = c;
        vga[off + 1] = 0x0F;
        shell_col++;
    }
    /* скролл */
    if (shell_row >= 25) {
        for (int i = 0; i < 24 * 80; i++) {
            vga[i * 2]     = vga[(i + 80) * 2];
            vga[i * 2 + 1] = vga[(i + 80) * 2 + 1];
        }
        for (int i = 0; i < 80; i++) {
            vga[(24 * 80 + i) * 2]     = ' ';
            vga[(24 * 80 + i) * 2 + 1] = 0x0F;
        }
        shell_row = 24;
    }
}

void shell_prompt() {
    /* Перед выводом приглашения гарантируем, что строка попадёт в окно */
    if (sb_line >= SCROLL_LINES) sb_line = SCROLL_LINES - 1;
    if (sb_line + 1 > scroll_total) {
        scroll_total = sb_line + 1;
        if (scroll_total > SCROLL_LINES) scroll_total = SCROLL_LINES;
    }
    print_color("colibri:", LIGHT_GREEN);
    cmd_pwd_no_newline();
    print("> ");
}

void shell_run() {
    while (1) {
        shell_prompt();
        input_len = 0;
        input_buf[0] = 0;

        while (1) {
            char c = get_key();

            if (c == KEY_UP || c == KEY_DOWN) {
                handle_scroll_key(c);
                continue;
            }

            if (c == '\n') {
                putchar('\n');
                input_buf[input_len] = 0;
                break;
            }
            if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    input_buf[input_len] = 0;
                    putchar('\b');
                }
                continue;
            }
            if (c == KEY_LEFT || c == KEY_RIGHT || c == KEY_TAB) continue;
            if ((unsigned char)c < 32) continue;
            if (input_len < LINE_MAX - 1) {
                input_buf[input_len++] = c;
                putchar(c);
            }
        }

        history_add(input_buf);
        shell_execute(input_buf);
    }
}

/* ============================================
   Main
   ============================================ */
void kernel_main() {
    clear_screen();
    draw_banner();
    heap_init();
    fs_init();
    proc_init();
    srand(12345);

    shell_run();
}
