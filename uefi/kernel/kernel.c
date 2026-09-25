/* ============================================
   Colibri OS — Kernel v0.9 (UEFI + Graphics)
   Desktop + Terminal + Cursor + Shell + FS
   ============================================ */

#include "kmalloc.h"

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;

/* ---------- BootInfo из UEFI-загрузчика ---------- */
typedef struct {
    uint32_t* framebuffer;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
    uint32_t  bpp;
} BootInfo;

static uint32_t* fb = 0;
static uint32_t  fb_width = 0;
static uint32_t  fb_height = 0;
static uint32_t  fb_pitch = 0;
static uint32_t  fb_bpp = 0;

/* ---------- Порты ввода-вывода ---------- */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ============================================
   Графика (framebuffer)
   ============================================ */
void putpixel(int x, int y, uint32_t col) {
    if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;
    uint32_t offset = y * (fb_pitch / 4) + x;
    fb[offset] = col;
}

void draw_rect(int x, int y, int w, int h, uint32_t col) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            putpixel(x + i, y + j, col);
}

void draw_border(int x, int y, int w, int h, uint32_t col) {
    for (int i = 0; i < w; i++) {
        putpixel(x + i, y, col);
        putpixel(x + i, y + h - 1, col);
    }
    for (int j = 0; j < h; j++) {
        putpixel(x, y + j, col);
        putpixel(x + w - 1, y + j, col);
    }
}

/* ============================================
   Шрифт 8x8
   ============================================ */
static const uint8_t font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x18,0x18,0x18,0x00,0x00,0x18,0x00},
    {0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00},
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
    {0x06,0x06,0x0C,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00},
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
};

void draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (c < 32 || c > 126) return;
    const uint8_t* glyph = font8x8[(int)c - 32];
    for (int j = 0; j < 8; j++) {
        uint8_t row = glyph[j];
        for (int i = 0; i < 8; i++) {
            if (row & (1 << i)) putpixel(x + i, y + j, fg);
            else putpixel(x + i, y + j, bg);
        }
    }
}

void draw_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*str) {
        if (*str == '\n') { cx = x; y += 8; }
        else { draw_char(cx, y, *str, fg, bg); cx += 8; }
        str++;
    }
}

/* ============================================
   Утилиты строк
   ============================================ */
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

/* ============================================
   Клавиатура
   ============================================ */
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
            uint8_t sc = inb(0x60);

            if (sc & 0x80) {
                uint8_t released = sc & 0x7F;
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
   Файловая система (RAM)
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
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE &&
            fs_objects[i].parent == parent &&
            strcmp(fs_objects[i].name, name) == 0) {
            return i;
        }
    }
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

    int colibri = 1;
    strcpy(fs_objects[colibri].name, "colibri");
    fs_objects[colibri].type = OBJ_DIR;
    fs_objects[colibri].parent = ROOT_INDEX;

    int users = 2;
    strcpy(fs_objects[users].name, "users");
    fs_objects[users].type = OBJ_DIR;
    fs_objects[users].parent = colibri;

    int alpha = 3;
    strcpy(fs_objects[alpha].name, "alpha");
    fs_objects[alpha].type = OBJ_DIR;
    fs_objects[alpha].parent = users;

    int system = 4;
    strcpy(fs_objects[system].name, "system");
    fs_objects[system].type = OBJ_DIR;
    fs_objects[system].parent = colibri;

    current_dir = alpha;
}

int fs_create_in(int parent_idx, const char* name, int type) {
    if (fs_find_in(parent_idx, name) != -1) return -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type == OBJ_FREE) {
            strcpy(fs_objects[i].name, name);
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
    if (fs_objects[idx].type == OBJ_DIR) {
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

void strip_quotes(const char* in, char* out) {
    int i = 0, j = 0;
    while (in[i] == ' ' || in[i] == '\t') i++;
    if (in[i] == '"') {
        i++;
        while (in[i] && in[i] != '"') out[j++] = in[i++];
    } else {
        while (in[i] && in[i] != ' ' && in[i] != '\t') out[j++] = in[i++];
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
                else {
                    int idx = fs_find_in(cur, part);
                    if (idx == -1) return -1;
                    cur = idx;
                }
            }
            if (*p == 0) break;
            pi = 0;
            p++;
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
                    else {
                        int idx = fs_find_in(last_parent, last_name);
                        if (idx == -1) return 0;
                        last_parent = idx;
                    }
                }
                strcpy(last_name, part);
            }
            if (*p == 0) break;
            pi = 0;
            p++;
            continue;
        }
        if (pi < FS_NAME_LEN - 1) part[pi++] = *p;
        p++;
    }

    *parent_idx = last_parent;
    strcpy(last, last_name);
    return (last_name[0] != 0);
}

/* ============================================
   Курсор (стрелка мыши)
   ============================================ */
#define CURSOR_W 12
#define CURSOR_H 19

static const uint8_t cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {2,0,0,0,0,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,1,2,0,0,0,0},
    {2,1,1,1,1,1,1,1,2,0,0,0},
    {2,1,1,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,1,1,1,1,2,0},
    {2,1,1,1,1,1,2,2,2,2,2,2},
    {2,1,1,2,1,1,2,0,0,0,0,0},
    {2,1,2,0,2,1,1,2,0,0,0,0},
    {2,2,0,0,2,1,1,2,0,0,0,0},
    {2,0,0,0,0,2,1,1,2,0,0,0},
    {0,0,0,0,0,2,1,1,2,0,0,0},
    {0,0,0,0,0,0,2,1,1,2,0,0},
    {0,0,0,0,0,0,2,2,2,0,0,0},
};

static int cursor_pos_x = 512;
static int cursor_pos_y = 384;
static int prev_cursor_x = -1;
static int prev_cursor_y = -1;

static uint32_t cursor_bg[CURSOR_H][CURSOR_W];

void cursor_hide() {
    if (prev_cursor_x < 0) return;
    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            int x = prev_cursor_x + i;
            int y = prev_cursor_y + j;
            if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height) continue;
            uint32_t offset = y * (fb_pitch / 4) + x;
            fb[offset] = cursor_bg[j][i];
        }
    }
}

void cursor_save_and_draw() {
    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            int x = cursor_pos_x + i;
            int y = cursor_pos_y + j;
            if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height) continue;
            uint32_t offset = y * (fb_pitch / 4) + x;
            cursor_bg[j][i] = fb[offset];
            if (cursor_bitmap[j][i] == 2) putpixel(x, y, 0x00000000);
            else if (cursor_bitmap[j][i] == 1) putpixel(x, y, 0x00FFFFFF);
        }
    }
    prev_cursor_x = cursor_pos_x;
    prev_cursor_y = cursor_pos_y;
}

/* ============================================
   Terminal (графический)
   ============================================ */
static int term_x = 0, term_y = 0;
static uint32_t term_fg = 0x00FFFFFF;
static uint32_t term_bg = 0x00000000;
static int term_max_x = 0, term_max_y = 0;
static int term_origin_x = 0, term_origin_y = 0;

void term_clear() {
    draw_rect(term_origin_x, term_origin_y, term_max_x * 8, term_max_y * 8, term_bg);
    term_x = 0; term_y = 0;
}

void term_putchar(char c) {
    if (c == '\n') {
        term_x = 0; term_y++;
        if (term_y >= term_max_y) term_clear();
        return;
    }
    if (c == '\b') {
        if (term_x > 0) {
            term_x--;
            draw_char(term_origin_x + term_x * 8, term_origin_y + term_y * 8, ' ', term_fg, term_bg);
        }
        return;
    }
    draw_char(term_origin_x + term_x * 8, term_origin_y + term_y * 8, c, term_fg, term_bg);
    term_x++;
    if (term_x >= term_max_x) { term_x = 0; term_y++; }
    if (term_y >= term_max_y) term_clear();
}

void term_print(const char* s) {
    while (*s) term_putchar(*s++);
}

void term_print_color(const char* s, uint32_t fg) {
    uint32_t old = term_fg;
    term_fg = fg;
    term_print(s);
    term_fg = old;
}

void term_print_dec(unsigned int n) {
    if (n == 0) { term_putchar('0'); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) term_putchar(buf[--i]);
}

/* ============================================
   Команды shell
   ============================================ */
static char cmd_buffer[128];

void cmd_help() {
    term_print("Commands:\n");
    term_print("  help       - this help\n");
    term_print("  ver        - version\n");
    term_print("  clear      - clear screen\n");
    term_print("  ls [path]  - list dir\n");
    term_print("  pwd        - current dir\n");
    term_print("  cd <path>  - change dir\n");
    term_print("  mkdir X    - make dir\n");
    term_print("  touch X    - create file\n");
    term_print("  rm X       - delete\n");
    term_print("  exit       - back to desktop\n");
}

void cmd_ver() {
    term_print("Colibri OS v0.9 (UEFI graphics + desktop + terminal)\n");
}

void cmd_pwd() {
    int stack[FS_MAX_OBJECTS];
    int top = 0;
    int cur = current_dir;
    while (cur != ROOT_INDEX) { stack[top++] = cur; cur = fs_objects[cur].parent; }
    term_print("/");
    while (top > 0) {
        int idx = stack[--top];
        term_print(fs_objects[idx].name);
        if (top > 0) term_print("/");
    }
    term_putchar('\n');
}

void cmd_ls(const char* path) {
    int dir = current_dir;
    if (path && path[0] != 0) {
        int idx = parse_path(path, current_dir);
        if (idx == -1 || fs_objects[idx].type != OBJ_DIR) {
            term_print("Not a directory: "); term_print(path); term_putchar('\n'); return;
        }
        dir = idx;
    }
    int count = 0;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == dir) {
            if (fs_objects[i].type == OBJ_DIR) {
                term_print_color("  ", 0x0080FFFF);
                term_print_color(fs_objects[i].name, 0x0080FFFF);
                term_print("/\n");
            } else {
                term_print("  ");
                term_print(fs_objects[i].name);
                term_print("  (");
                term_print_dec(fs_objects[i].size);
                term_print(" bytes)\n");
            }
            count++;
        }
    }
    if (count == 0) term_print("  (empty)\n");
}

void parse_command() {
    if (cmd_buffer[0] == 0) return;

    if (strcmp(cmd_buffer, "help") == 0) { cmd_help(); return; }
    if (strcmp(cmd_buffer, "ver") == 0) { cmd_ver(); return; }
    if (strcmp(cmd_buffer, "clear") == 0) { term_clear(); return; }
    if (strcmp(cmd_buffer, "pwd") == 0) { cmd_pwd(); return; }
    if (strcmp(cmd_buffer, "ls") == 0) { cmd_ls(0); return; }

    if (strncmp(cmd_buffer, "ls ", 3) == 0) { cmd_ls(cmd_buffer + 3); return; }

    if (strncmp(cmd_buffer, "mkdir ", 6) == 0) {
        int parent; char last[FS_NAME_LEN];
        if (!split_path(cmd_buffer + 6, current_dir, &parent, last)) { term_print("Usage: mkdir <path>\n"); return; }
        if (fs_create_in(parent, last, OBJ_DIR) != -1) { term_print("Directory created\n"); }
        else term_print("Failed\n");
        return;
    }

    if (strncmp(cmd_buffer, "touch ", 6) == 0) {
        int parent; char last[FS_NAME_LEN];
        if (!split_path(cmd_buffer + 6, current_dir, &parent, last)) { term_print("Usage: touch <path>\n"); return; }
        if (fs_create_in(parent, last, OBJ_FILE) != -1) { term_print("File created\n"); }
        else term_print("Failed\n");
        return;
    }

    if (strncmp(cmd_buffer, "rm ", 3) == 0) {
        int idx = parse_path(cmd_buffer + 3, current_dir);
        if (idx == -1 || idx == ROOT_INDEX) { term_print("Not found\n"); return; }
        fs_delete_by_index(idx);
        term_print("Deleted\n");
        return;
    }

    if (strncmp(cmd_buffer, "cd ", 3) == 0) {
        int idx = parse_path(cmd_buffer + 3, current_dir);
        if (idx == -1 || fs_objects[idx].type != OBJ_DIR) { term_print("Not a dir\n"); return; }
        current_dir = idx;
        return;
    }

    term_print("Unknown: "); term_print(cmd_buffer); term_putchar('\n');
}

void term_read_line() {
    int i = 0;
    while (1) {
        char c = get_key();

        cursor_hide();
        cursor_save_and_draw();

        if (c == KEY_ESC) { cmd_buffer[0] = 0; return; }
        if (c == '\n') {
            cmd_buffer[i] = 0;
            term_putchar('\n');
            return;
        }
        if (c == '\b') {
            if (i > 0) { i--; term_putchar('\b'); }
            continue;
        }
        if (i < 127) {
            cmd_buffer[i++] = c;
            term_putchar(c);
        }
    }
}

/* ============================================
   Приложение: Terminal
   ============================================ */
void app_terminal() {
    draw_rect(0, 0, fb_width, fb_height, 0x00000000);

    draw_rect(0, 0, fb_width, 20, 0x00000080);
    draw_text(8, 6, "Colibri Terminal  (Esc = back to desktop)", 0x00FFFFFF, 0x00000080);

    term_origin_x = 8;
    term_origin_y = 30;
    term_max_x = (fb_width - 16) / 8;
    term_max_y = (fb_height - 40) / 8;
    term_fg = 0x00FFFFFF;
    term_bg = 0x00000000;

    term_clear();
    term_print_color("Colibri OS v0.9 - Terminal\n", 0x0000FF00);
    term_print("Type 'help' for commands, 'exit' to return.\n\n");

    while (1) {
        term_print_color("colibri:> ", 0x0000FF00);
        term_read_line();
        if (cmd_buffer[0] == 0) return;
        if (strcmp(cmd_buffer, "exit") == 0) return;
        parse_command();

        cursor_hide();
        cursor_save_and_draw();
    }
}

/* ============================================
   Desktop
   ============================================ */
void draw_button(int x, int y, int w, int h, const char* label, uint32_t bg) {
    draw_rect(x, y, w, h, bg);
    draw_border(x, y, w, h, 0x00FFFFFF);
    draw_text(x + 8, y + 8, label, 0x00FFFFFF, bg);
}

void app_desktop() {
    while (1) {
        draw_rect(0, 0, fb_width, fb_height, 0x00000080);

        draw_rect(0, 0, fb_width, 30, 0x000080FF);
        draw_text(10, 10, "Colibri OS v0.9 - Desktop", 0x00FFFFFF, 0x000080FF);

        draw_button(50,  80,  240, 50, "[1] Terminal", 0x00404040);
        draw_button(50,  150, 240, 50, "[2] Files",    0x00404040);
        draw_button(50,  220, 240, 50, "[3] About",    0x00404040);
        draw_button(50,  290, 240, 50, "[4] Reboot",   0x00404040);

        draw_text(50, fb_height - 30, "Press 1-4 to open app", 0x00FFFFFF, 0x00000080);

        cursor_save_and_draw();
        char c = get_key();
        cursor_hide();

        if (c == '1') {
            app_terminal();
        } else if (c == '2') {
            draw_rect(0, 0, fb_width, fb_height, 0x00000000);
            draw_text(20, 20, "Files app — not implemented yet", 0x00FFFFFF, 0x00000000);
            draw_text(20, 40, "Press any key to return", 0x00A0A0A0, 0x00000000);
            get_key();
        } else if (c == '3') {
            draw_rect(0, 0, fb_width, fb_height, 0x00000000);
            draw_text(20, 20, "Colibri OS v0.9", 0x0000FF00, 0x00000000);
            draw_text(20, 40, "Hobby OS by Asde LLC", 0x00FFFFFF, 0x00000000);
            draw_text(20, 60, "UEFI graphics + 32-bit protected mode", 0x00FFFFFF, 0x00000000);
            draw_text(20, 80, "Press any key to return", 0x00A0A0A0, 0x00000000);
            get_key();
        } else if (c == '4') {
            while (inb(0x64) & 0x01) inb(0x60);
            outb(0x64, 0xFE);
        }
    }
}

/* ============================================
   Точка входа ядра
   ============================================ */
void kernel_main(BootInfo* info) {
    fb        = info->framebuffer;
    fb_width  = info->width;
    fb_height = info->height;
    fb_pitch  = info->pitch;
    fb_bpp    = info->bpp;

    heap_init();
    fs_init();

    if (fb == 0 || fb_width == 0 || fb_height == 0) {
        while (1) {}
    }

    app_desktop();
}