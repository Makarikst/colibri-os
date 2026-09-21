/* ============================================
   Colibri OS — Ядро на C
   VGA + клавиатура + Shell + ФС в RAM
   ============================================ */

#define VGA_MEMORY 0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define BLACK        0
#define WHITE        15
#define LIGHT_GREEN  10
#define LIGHT_CYAN   11

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

/* ---------- Файловая система ---------- */
#define FS_MAX_FILES 10
#define FS_NAME_LEN  32

typedef struct {
    char name[FS_NAME_LEN];
    int  used;
} File;

static File fs_files[FS_MAX_FILES];

void fs_init() {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_files[i].used = 0;
        fs_files[i].name[0] = 0;
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
            return 1;
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
            putchar('\n');
            count++;
        }
    }
    if (count == 0) print("  (empty)\n");
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
    print("  help     - this help\n");
    print("  about    - about Colibri OS\n");
    print("  ver      - version\n");
    print("  clear    - clear screen\n");
    print("  echo X   - print X\n");
    print("  ls       - list files\n");
    print("  touch X  - create file X\n");
    print("  rm X     - delete file X\n");
    print("  reboot   - reboot\n");
}

void cmd_about() {
    print("Colibri OS - hobby OS by Asde LLC\n");
    print("Written in C, running in 32-bit mode\n");
}

void cmd_ver() {
    print("Colibri OS v0.3 (C kernel)\n");
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
    print_color("Colibri OS v0.3\n", LIGHT_CYAN);
    print_color("================\n", LIGHT_CYAN);
    print("C kernel loaded successfully!\n");
    print("Type 'help' for commands.\n\n");
}

void kernel_main() {
    clear_screen();
    fs_init();
    print_banner();
    shell_loop();
}