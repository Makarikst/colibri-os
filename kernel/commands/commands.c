#include "commands.h"
#include "../shell/shell.h"

/* ============================================
   Env
   ============================================ */
#define ENV_MAX 16
typedef struct { char key[32]; char val[64]; } EnvVar;
static EnvVar env_vars[ENV_MAX];
static int env_count = 0;

static void env_set(const char* k, const char* v) {
    for (int i = 0; i < env_count; i++)
        if (strcmp(env_vars[i].key, k) == 0) { strncpy(env_vars[i].val, v, 63); return; }
    if (env_count < ENV_MAX) {
        strncpy(env_vars[env_count].key, k, 31);
        strncpy(env_vars[env_count].val, v, 63);
        env_count++;
    }
}

static void env_unset(const char* k) {
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

static const char* alias_get(const char* name) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, name) == 0) return aliases[i].cmd;
    return 0;
}

static void alias_set(const char* name, const char* cmd) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, name) == 0) { strncpy(aliases[i].cmd, cmd, 127); return; }
    if (alias_count < ALIAS_MAX) {
        strncpy(aliases[alias_count].name, name, 15);
        strncpy(aliases[alias_count].cmd, cmd, 127);
        alias_count++;
    }
}

static void alias_unset(const char* name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            for (int j = i; j < alias_count - 1; j++) aliases[j] = aliases[j+1];
            alias_count--;
            return;
        }
    }
}

/* Публичные обёртки для shell.c */
const char* commands_alias_get(const char* name) { return alias_get(name); }
void commands_env_show(void) { cmd_env(); }
void commands_alias_show(void) { cmd_alias_list(); }

/* ============================================
   Processes (stub)
   ============================================ */
#define PROC_MAX 8
typedef struct { int pid; int active; char name[32]; } Proc;
static Proc procs[PROC_MAX];
static int next_pid = 1;

void proc_init(void) {
    for (int i = 0; i < PROC_MAX; i++) procs[i].active = 0;
    next_pid = 1;
}

static int proc_spawn(const char* name) {
    for (int i = 0; i < PROC_MAX; i++)
        if (!procs[i].active) {
            procs[i].active = 1;
            procs[i].pid = next_pid++;
            strncpy(procs[i].name, name, 31);
            return procs[i].pid;
        }
    return -1;
}

static int proc_kill(int pid) {
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
   Parser helpers (нужны для tokenize)
   ============================================ */
static char expr_buf[256];
static char* expr_buf_ptr = expr_buf;   /* используется в cmd_calc */
/* ============================================
   System
   ============================================ */
void cmd_help(void) {
    vga_print_color("=== Colibri OS v0.9 - Commands ===\n", VGA_LIGHT_CYAN);
    vga_print_color("--- System ---\n", VGA_YELLOW);
    vga_print("  help          - this help\n");
    vga_print("  ver           - version\n");
    vga_print("  banner        - show banner\n");
    vga_print("  clear         - clear screen\n");
    vga_print("  date          - date & time\n");
    vga_print("  uptime        - time since boot\n");
    vga_print("  mem           - memory info\n");
    vga_print("  heap          - heap stats\n");
    vga_print("  history       - last commands\n");
    vga_print("  color <n>     - text color (0-15)\n");
    vga_print("  reboot        - restart\n");
    vga_print("  shutdown      - halt CPU\n");
    vga_print_color("--- Executable ---\n", VGA_YELLOW);
    vga_print("  nano X        - text editor\n");
    vga_print("  cpe X         - CPE run X\n");
    vga_print("  run X         - Run app (v1.0) X\n");
    vga_print_color("--- Files ---\n", VGA_YELLOW);
    vga_print("  ls [path]     - list dir\n");
    vga_print("  pwd           - current dir\n");
    vga_print("  cd <path>     - change dir\n");
    vga_print("  mkdir X       - make dir\n");
    vga_print("  rmdir X       - remove dir\n");
    vga_print("  touch X       - create file\n");
    vga_print("  rm X          - delete\n");
    vga_print("  cat <file>    - show file\n");
    vga_print("  echo X > Y    - write X to Y\n");
    vga_print("  echo X >> Y   - append X to Y\n");
    vga_print("  write <f> <t> - write text\n");
    vga_print("  append <f> <t>- append text\n");
    vga_print("  trunc <file>  - clear file\n");
    vga_print("  cp <src> <dst>- copy file\n");
    vga_print("  mv <src> <dst>- rename\n");
    vga_print("  tree          - show tree\n");
    vga_print("  find <name>   - find file\n");
    vga_print("  stat <file>   - file info\n");
    vga_print("  wc <file>     - lines/words/chars\n");
    vga_print("  head <file>   - first 10 lines\n");
    vga_print("  tail <file>   - last 10 lines\n");
    vga_print("  grep <pat> <f>- search in file\n");
    vga_print("  sort <file>   - sort lines\n");
    vga_print("  hexdump <file>- hex dump\n");
    vga_print_color("--- Math ---\n", VGA_YELLOW);
    vga_print("  calc <expr>   - + - * / %% ^\n");
    vga_print("  sqrt <n>      - square root\n");
    vga_print("  pow <a> <b>   - a^b\n");
    vga_print("  rand          - random 0-99\n");
    vga_print("  prime <n>     - is prime?\n");
    vga_print("  gcd <a> <b>   - GCD\n");
    vga_print("  lcm <a> <b>   - LCM\n");
    vga_print("  hex <n>       - decimal to hex\n");
    vga_print("  bin <n>       - decimal to binary\n");
    vga_print_color("--- Misc ---\n", VGA_YELLOW);
    vga_print("  echo X        - print X\n");
    vga_print("  sleep <ms>    - pause\n");
    vga_print("  beep          - PC speaker beep\n");
    vga_print_color("--- Process (stub) ---\n", VGA_YELLOW);
    vga_print("  ps            - list processes\n");
    vga_print("  spawn <name>  - spawn process\n");
    vga_print("  kill <pid>    - kill process\n");
    vga_print_color("--- Net (stub) ---\n", VGA_YELLOW);
    vga_print("  ifconfig      - show interfaces\n");
    vga_print("  ping <host>   - ping host\n");
    vga_print_color("--- Env / Alias ---\n", VGA_YELLOW);
    vga_print("  env           - show vars\n");
    vga_print("  setenv K=V    - set var\n");
    vga_print("  unsetenv K    - unset var\n");
    vga_print("  alias         - list aliases\n");
    vga_print("  alias X=Y     - set alias\n");
    vga_print("  unalias X     - remove alias\n");
    vga_print("\n");
}

void cmd_ver(void) { vga_print("Colibri OS v0.9 (full, with utils)\n"); }

void cmd_banner(void) { vga_banner(); }

void cmd_clear(void) { vga_clear(); vga_banner(); }

void cmd_pwd_no_newline(void) {
    int stack[FS_MAX_OBJECTS];
    int top = 0, cur = fs_get_current_dir();
    while (cur != ROOT_INDEX) { stack[top++] = cur; cur = fs_get(cur)->parent; }
    vga_print("/");
    while (top > 0) {
        int idx = stack[--top];
        vga_print(fs_get(idx)->name);
        if (top > 0) vga_print("/");
    }
}

void cmd_pwd(void) { cmd_pwd_no_newline(); vga_putchar('\n'); }

void cmd_date(void) {
    rtc_time_t t = rtc_get_time();
    vga_print_dec(t.day); vga_putchar('.');
    if (t.mon < 10) vga_putchar('0');
    vga_print_dec(t.mon); vga_putchar('.');
    vga_print_dec(t.year); vga_putchar(' ');
    if (t.hour < 10) vga_putchar('0');
    vga_print_dec(t.hour); vga_putchar(':');
    if (t.min < 10) vga_putchar('0');
    vga_print_dec(t.min); vga_putchar(':');
    if (t.sec < 10) vga_putchar('0');
    vga_print_dec(t.sec);
    vga_putchar('\n');
}

void cmd_uptime(void) {
    rtc_time_t t = rtc_get_time();
    vga_print("Current time: ");
    if (t.hour < 10) vga_putchar('0');
    vga_print_dec(t.hour); vga_putchar(':');
    if (t.min < 10) vga_putchar('0');
    vga_print_dec(t.min); vga_putchar(':');
    if (t.sec < 10) vga_putchar('0');
    vga_print_dec(t.sec);
    vga_putchar('\n');
}

void cmd_mem(void) {
    vga_print("Heap start: "); vga_print_hex32(0x200000);
    vga_print("\nHeap size:  1048576 bytes\n");
    vga_print("VGA:        text mode 80x25 @ 0xB8000\n");
}

void cmd_heap(void) { heap_stats(); }

void cmd_color(const char* arg) {
    int n = atoi(arg);
    if (n < 0 || n > 15) { vga_print("0-15\n"); return; }
    vga_set_color((unsigned char)n);
}

void cmd_sleep(const char* arg) {
    int ms = atoi(arg);
    if (ms < 0) ms = 0;
    for (volatile int i = 0; i < ms * 1000; i++) { }
    vga_print("Slept ");
    vga_print_dec(ms);
    vga_print(" ms\n");
}

void cmd_beep(void) {
    outb(0x43, 0xB6);
    unsigned int div = 1193180 / 440;
    outb(0x42, (unsigned char)(div & 0xFF));
    outb(0x42, (unsigned char)((div >> 8) & 0xFF));
    outb(0x61, inb(0x61) | 3);
    for (volatile int i = 0; i < 2000000; i++) { }
    outb(0x61, inb(0x61) & ~3);
    vga_print("Beep!\n");
}

void cmd_reboot(void) {
    vga_print("Rebooting...\n");
    while (inb(0x64) & 0x02) { }
    outb(0x64, 0xFE);
    cpu_cli();
    while (1) cpu_halt();
}

void cmd_shutdown(void) {
    vga_print("Shutting down...\n");
    outb(0x604, 0x00);
    outb(0x604, 0x20);
    outb(0xB004, 0x00);
    outb(0xB004, 0x20);
    cpu_cli();
    while (1) cpu_halt();
}
/* ============================================
   Files
   ============================================ */
void cmd_ls(const char* path) {
    int dir = fs_get_current_dir();
    if (path && path[0] != 0) {
        int idx = parse_path(path, fs_get_current_dir());
        if (idx == -1 || fs_get(idx)->type != OBJ_DIR) { vga_print("Not a directory\n"); return; }
        dir = idx;
    }
    int count = 0;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (o->type != OBJ_FREE && o->parent == dir) {
            if (o->type == OBJ_DIR) {
                vga_print_color("  ", VGA_LIGHT_BLUE);
                vga_print_color(o->name, VGA_LIGHT_BLUE);
                vga_print("/\n");
            } else {
                vga_print("  "); vga_print(o->name);
                vga_print("  ("); vga_print_dec(o->size); vga_print(" bytes)\n");
            }
            count++;
        }
    }
    if (count == 0) vga_print("  (empty)\n");
}

void cmd_cat(const char* path) {
    int idx = parse_path(path, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    vga_print(o->data);
    if (o->size > 0 && o->data[o->size - 1] != '\n') vga_putchar('\n');
}

void cmd_echo_redirect(const char* text, const char* filename, int append) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(filename, fs_get_current_dir(), &parent, last)) { vga_print("Invalid path\n"); return; }
    int idx = fs_find_in(parent, last);
    if (idx == -1) {
        idx = fs_create_in(parent, last, OBJ_FILE);
        if (idx == -1) { vga_print("Cannot create file\n"); return; }
    }
    FsObject* o = fs_get(idx);
    int size = o->size;
    if (size < 0 || size > FS_DATA_LEN) size = 0;
    int tlen = (int)strlen(text);
    if (tlen < 0) tlen = 0;
    int base = append ? size : 0;
    if (base < 0) base = 0;
    if (base > FS_DATA_LEN - 2) base = FS_DATA_LEN - 2;
    int max_copy = FS_DATA_LEN - 2 - base;
    if (max_copy < 0) max_copy = 0;
    if (tlen > max_copy) tlen = max_copy;
    if (tlen > 0) strncpy(o->data + base, text, tlen);
    o->data[base + tlen] = '\n';
    o->size = base + tlen + 1;
    vga_print(append ? "Appended " : "Written ");
    vga_print_dec(tlen + 1); vga_print(" bytes to "); vga_print(filename); vga_putchar('\n');
}

void cmd_cp(const char* src, const char* dst) {
    int sidx = parse_path(src, fs_get_current_dir());
    if (sidx == -1 || fs_get(sidx)->type != OBJ_FILE) { vga_print("Source not found\n"); return; }
    int parent; char last[FS_NAME_LEN];
    if (!split_path(dst, fs_get_current_dir(), &parent, last)) { vga_print("Invalid dst\n"); return; }
    int didx = fs_find_in(parent, last);
    if (didx == -1) {
        didx = fs_create_in(parent, last, OBJ_FILE);
        if (didx == -1) { vga_print("Cannot create dst\n"); return; }
    }
    FsObject* s = fs_get(sidx);
    FsObject* d = fs_get(didx);
    strncpy(d->data, s->data, FS_DATA_LEN);
    d->size = s->size;
    vga_print("Copied.\n");
}

void cmd_mv(const char* src, const char* dst) {
    int sidx = parse_path(src, fs_get_current_dir());
    if (sidx == -1) { vga_print("Source not found\n"); return; }
    int parent; char last[FS_NAME_LEN];
    if (!split_path(dst, fs_get_current_dir(), &parent, last)) { vga_print("Invalid dst\n"); return; }
    if (fs_find_in(parent, last) != -1) { vga_print("Dst exists\n"); return; }
    FsObject* s = fs_get(sidx);
    strncpy(s->name, last, FS_NAME_LEN - 1);
    s->parent = parent;
    vga_print("Moved.\n");
}

void cmd_touch(const char* name) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(name, fs_get_current_dir(), &parent, last)) { vga_print("Invalid name\n"); return; }
    if (fs_find_in(parent, last) != -1) { vga_print("Exists\n"); return; }
    if (fs_create_in(parent, last, OBJ_FILE) == -1) vga_print("Cannot create\n");
    else vga_print("Created.\n");
}

void cmd_mkdir(const char* name) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(name, fs_get_current_dir(), &parent, last)) { vga_print("Invalid name\n"); return; }
    if (fs_find_in(parent, last) != -1) { vga_print("Exists\n"); return; }
    if (fs_create_in(parent, last, OBJ_DIR) == -1) vga_print("Cannot create\n");
    else vga_print("Created dir.\n");
}

void cmd_rm(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1) { vga_print("Not found\n"); return; }
    if (idx == ROOT_INDEX) { vga_print("Cannot remove root\n"); return; }
    if (idx == fs_get_current_dir()) { vga_print("Cannot remove current dir\n"); return; }
    fs_delete_by_index(idx);
    vga_print("Removed.\n");
}

void cmd_rmdir(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1) { vga_print("Not found\n"); return; }
    if (fs_get(idx)->type != OBJ_DIR) { vga_print("Not a dir\n"); return; }
    if (idx == fs_get_current_dir()) { vga_print("Cannot remove current dir\n"); return; }
    fs_delete_by_index(idx);
    vga_print("Removed dir.\n");
}

void cmd_cd(const char* path) {
    int idx = parse_path(path, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_DIR) { vga_print("Not a directory\n"); return; }
    fs_set_current_dir(idx);
}

void cmd_tree(void) { tree_recursive(fs_get_current_dir(), 0); }

void cmd_find(const char* name) {
    int found = 0;
    find_recursive(fs_get_current_dir(), name, &found);
    if (!found) vga_print("Not found\n");
}

void cmd_stat(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1) { vga_print("Not found\n"); return; }
    FsObject* o = fs_get(idx);
    vga_print("Name:   "); vga_print(o->name); vga_putchar('\n');
    vga_print("Type:   "); vga_print(o->type == OBJ_DIR ? "dir\n" : "file\n");
    vga_print("Size:   "); vga_print_dec(o->size); vga_print(" bytes\n");
    vga_print("Parent: "); vga_print_dec(o->parent); vga_putchar('\n');
}

void cmd_wc(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    int lines = 0, words = 0, chars = o->size;
    int in_word = 0;
    for (int i = 0; i < o->size; i++) {
        char c = o->data[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n') in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    vga_print_dec(lines); vga_print(" lines, ");
    vga_print_dec(words); vga_print(" words, ");
    vga_print_dec(chars); vga_print(" bytes\n");
}

void cmd_head(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    int lines = 0;
    for (int i = 0; i < o->size && lines < 10; i++) {
        vga_putchar(o->data[i]);
        if (o->data[i] == '\n') lines++;
    }
    if (lines == 0 || o->data[o->size-1] != '\n') vga_putchar('\n');
}

void cmd_tail(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    int total = 0;
    for (int i = 0; i < o->size; i++)
        if (o->data[i] == '\n') total++;
    int start_line = total > 10 ? total - 10 : 0;
    int cur = 0;
    for (int i = 0; i < o->size; i++) {
        if (cur >= start_line) vga_putchar(o->data[i]);
        if (o->data[i] == '\n') cur++;
    }
    vga_putchar('\n');
}

void cmd_grep(const char* pat, const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    char line[256];
    int li = 0;
    for (int i = 0; i <= o->size; i++) {
        char c = (i < o->size) ? o->data[i] : '\n';
        if (c == '\n') {
            line[li] = 0;
            if (li > 0 && strstr(line, pat)) { vga_print(line); vga_putchar('\n'); }
            li = 0;
        } else if (li < 255) line[li++] = c;
    }
}

void cmd_sort(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    char lines[64][128];
    int lc = 0, li = 0;
    for (int i = 0; i <= o->size && lc < 64; i++) {
        char c = (i < o->size) ? o->data[i] : '\n';
        if (c == '\n') { lines[lc][li] = 0; lc++; li = 0; }
        else if (li < 127) lines[lc][li++] = c;
    }
    for (int i = 0; i < lc - 1; i++)
        for (int j = 0; j < lc - 1 - i; j++)
            if (strcmp(lines[j], lines[j+1]) > 0) {
                char tmp[128];
                strcpy(tmp, lines[j]);
                strcpy(lines[j], lines[j+1]);
                strcpy(lines[j+1], tmp);
            }
    for (int i = 0; i < lc; i++) { vga_print(lines[i]); vga_putchar('\n'); }
}

void cmd_hexdump(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    dump_hex(o->data, o->size);
}

void cmd_trunc(const char* name) {
    int idx = parse_path(name, fs_get_current_dir());
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) { vga_print("File not found\n"); return; }
    FsObject* o = fs_get(idx);
    o->size = 0;
    o->data[0] = 0;
    vga_print("Truncated.\n");
}

void cmd_write(const char* name, const char* text)  { cmd_echo_redirect(text, name, 0); }
void cmd_append(const char* name, const char* text) { cmd_echo_redirect(text, name, 1); }
/* ============================================
   Math
   ============================================ */
/* Калькулятор: переписан из kernel.c. Использует рекурсивный парсер */
static int calc_parse_primary(const char** p, int* ok);
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
        while (*(*p) >= '0' && *(*p) <= '9') { value = value * 10 + (*(*p) - '0'); (*p)++; }
        return value;
    }
    *ok = 0; return 0;
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
        else if (op == '/') { if (rhs == 0) { *ok = 0; return 0; } value /= rhs; }
        else { if (rhs == 0) { *ok = 0; return 0; } value %= rhs; }
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
        if (op == '+') value += rhs; else value -= rhs;
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

void cmd_calc(const char* expr) {
    int ok;
    int r = calc_expr(expr, &ok);
    if (!ok) { vga_print("Error\n"); return; }
    vga_print("= "); vga_print_dec(r); vga_putchar('\n');
}

void cmd_sqrt(const char* arg) {
    int n = atoi(arg);
    vga_print("sqrt("); vga_print_dec(n); vga_print(") = ");
    vga_print_dec(sqrt_int(n)); vga_putchar('\n');
}

void cmd_pow(const char* a, const char* b) {
    int x = atoi(a), y = atoi(b);
    vga_print_dec(x); vga_print("^"); vga_print_dec(y); vga_print(" = ");
    vga_print_dec(pow_int(x, y)); vga_putchar('\n');
}

void cmd_rand(void) { vga_print_dec(rand_range(0, 99)); vga_putchar('\n'); }

void cmd_prime(const char* arg) {
    int n = atoi(arg);
    if (is_prime(n)) { vga_print_dec(n); vga_print(" is prime\n"); }
    else             { vga_print_dec(n); vga_print(" is not prime\n"); }
}

void cmd_gcd(const char* a, const char* b) { vga_print_dec(gcd(atoi(a), atoi(b))); vga_putchar('\n'); }
void cmd_lcm(const char* a, const char* b) { vga_print_dec(lcm(atoi(a), atoi(b))); vga_putchar('\n'); }

void cmd_hex(const char* arg) {
    int n = atoi(arg);
    char buf[16];
    itoa(n, buf, 16);
    vga_print("0x"); vga_print(buf); vga_putchar('\n');
}

void cmd_bin(const char* arg) {
    int n = atoi(arg);
    char buf[40];
    itoa(n, buf, 2);
    vga_print(buf); vga_putchar('\n');
}

void cmd_echo_args(int n, char tokens[][128]) {
    for (int i = 1; i < n; i++) { if (i > 1) vga_putchar(' '); vga_print(tokens[i]); }
    vga_putchar('\n');
}

/* ============================================
   Env / Alias
   ============================================ */
void cmd_env(void) {
    if (env_count == 0) { vga_print("(no vars)\n"); return; }
    for (int i = 0; i < env_count; i++) {
        vga_print(env_vars[i].key);
        vga_print("=");
        vga_print(env_vars[i].val);
        vga_putchar('\n');
    }
}

void cmd_setenv(const char* kv) {
    char key[32], val[64];
    int i = 0, j = 0;
    while (kv[i] && kv[i] != '=' && j < 31) key[j++] = kv[i++];
    key[j] = 0;
    if (kv[i] != '=') { vga_print("Usage: setenv K=V\n"); return; }
    i++; j = 0;
    while (kv[i] && j < 63) val[j++] = kv[i++];
    val[j] = 0;
    env_set(key, val);
    vga_print("OK\n");
}

void cmd_unsetenv(const char* key) { env_unset(key); vga_print("OK\n"); }

void cmd_alias_list(void) {
    if (alias_count == 0) { vga_print("(no aliases)\n"); return; }
    for (int i = 0; i < alias_count; i++) {
        vga_print(aliases[i].name);
        vga_print(" = ");
        vga_print(aliases[i].cmd);
        vga_putchar('\n');
    }
}

void cmd_alias_set(const char* kv) {
    char name[16], cmd[128];
    int i = 0, j = 0;
    while (kv[i] && kv[i] != '=' && j < 15) name[j++] = kv[i++];
    name[j] = 0;
    if (kv[i] != '=') { vga_print("Usage: alias X=Y\n"); return; }
    i++; j = 0;
    while (kv[i] && j < 127) cmd[j++] = kv[i++];
    cmd[j] = 0;
    alias_set(name, cmd);
    vga_print("OK\n");
}

void cmd_unalias(const char* name) { alias_unset(name); vga_print("OK\n"); }

/* ============================================
   Process
   ============================================ */
void cmd_ps(void) {
    int any = 0;
    vga_print("PID  NAME\n");
    for (int i = 0; i < PROC_MAX; i++)
        if (procs[i].active) {
            vga_print_dec(procs[i].pid); vga_print("    ");
            vga_print(procs[i].name); vga_putchar('\n');
            any = 1;
        }
    if (!any) vga_print("(no processes)\n");
}

void cmd_spawn(const char* name) {
    int pid = proc_spawn(name);
    if (pid < 0) { vga_print("No slots\n"); return; }
    vga_print("Spawned PID "); vga_print_dec(pid); vga_putchar('\n');
}

void cmd_kill(const char* arg) {
    int pid = atoi(arg);
    if (proc_kill(pid)) vga_print("Killed\n");
    else vga_print("No such process\n");
}

/* ============================================
   Net
   ============================================ */
void cmd_ifconfig(void) {
    vga_print("eth0: ");
    vga_print(net_up ? "UP" : "DOWN");
    vga_putchar('\n');
    vga_print("  ip:   "); vga_print(net_ip); vga_putchar('\n');
    vga_print("  mask: "); vga_print(net_mask); vga_putchar('\n');
    vga_print("  gw:   "); vga_print(net_gw); vga_putchar('\n');
}

void cmd_ping(const char* host) {
    vga_print("PING ");
    vga_print(host);
    vga_print(" (stub): 4 packets, 0% loss\n");
}
