#include "desktop.h"
#include "icons.h"
#include "bird.h"
#include "../fs/fs.h"
#include "../utils/utils.h"

DesktopIcon desktop_icons[DESKTOP_MAX_ICONS];
int         desktop_icons_count = 0;

/* ============================================
   Инициализация
   ============================================ */
void desktop_icons_init(void) {
    for (int i = 0; i < DESKTOP_MAX_ICONS; i++) {
        desktop_icons[i].fs_index = -1;
        desktop_icons[i].x = 0;
        desktop_icons[i].y = 0;
        desktop_icons[i].dragging = 0;
        desktop_icons[i].drag_dx = 0;
        desktop_icons[i].drag_dy = 0;
        desktop_icons[i].moved = 0;
    }
    desktop_icons_count = 0;
}

int desktop_get_folder_index(void) {
    int colibri = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == ROOT_INDEX && strcmp(o->name, "colibri") == 0) colibri = i;
    }
    if (colibri < 0) return -1;

    int users = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == colibri && strcmp(o->name, "users") == 0) users = i;
    }
    if (users < 0) return -1;

    int alpha = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == users && strcmp(o->name, "alpha") == 0) alpha = i;
    }
    if (alpha < 0) return -1;

    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == alpha && strcmp(o->name, "Desktop") == 0) return i;
    }
    return -1;
}

int desktop_get_trash_index(void) {
    int colibri = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == ROOT_INDEX && strcmp(o->name, "colibri") == 0) colibri = i;
    }
    if (colibri < 0) return -1;

    int users = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == colibri && strcmp(o->name, "users") == 0) users = i;
    }
    if (users < 0) return -1;

    int alpha = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == users && strcmp(o->name, "alpha") == 0) alpha = i;
    }
    if (alpha < 0) return -1;

    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == alpha && strcmp(o->name, "Trash") == 0) return i;
    }
    return -1;
}

void desktop_scan_files(void) {
    int old_count = desktop_icons_count;
    DesktopIcon old_icons[DESKTOP_MAX_ICONS];
    for (int i = 0; i < old_count; i++) old_icons[i] = desktop_icons[i];

    desktop_icons_count = 0;

    int desktop_idx = desktop_get_folder_index();
    if (desktop_idx < 0) return;

    int base_y = 16;
    int idx = 0;

    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type == OBJ_FREE) continue;
        if (o->parent != desktop_idx) continue;
        if (desktop_icons_count >= DESKTOP_MAX_ICONS) break;

        DesktopIcon* ic = &desktop_icons[desktop_icons_count];
        ic->fs_index = i;
        ic->dragging = 0;
        ic->moved = 0;

        int found = 0;
        for (int j = 0; j < old_count; j++) {
            if (old_icons[j].fs_index == i) {
                ic->x = old_icons[j].x;
                ic->y = old_icons[j].y;
                found = 1;
                break;
            }
        }
        if (!found) {
            ic->x = ICON_TERMINAL_X;
            ic->y = base_y + idx * 64;
            idx++;
        }

        desktop_icons_count++;
    }
}

/* ============================================
   Вспомогательные рисования
   ============================================ */
static void draw_image_32x32_buf(uint32_t* buf, int bw, int bh,
                                  const uint32_t img[32][32],
                                  int x, int y) {
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            uint32_t c = img[row][col];
            if (c == 0) continue;
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= bw) continue;
            if (py < 0 || py >= bh) continue;
            buf[py * bw + px] = c;
        }
    }
}

static void draw_bird_24x24(uint32_t x, uint32_t y) {
    for (int row = 0; row < 24; row++)
        for (int col = 0; col < 24; col++) {
            uint32_t c = bird_24x24[row][col];
            if (c != 0) fb_putpixel(x + col, y + row, c);
        }
}

static uint32_t text_width(const char* str) { return strlen(str) * 8; }

static void icon_rect(DesktopIcon* ic, int* rx, int* ry, int* rw, int* rh) {
    *rx = ic->x;
    *ry = ic->y;
    *rw = DESKTOP_ICON_SIZE;
    *rh = DESKTOP_ICON_IMG + DESKTOP_ICON_TEXT;
}

void desktop_draw_background(void) { fb_clear(DESKTOP_BG); }

void desktop_draw_panel(void) {
    uint32_t py = fb_info.height - PANEL_HEIGHT;
    fb_rect_fill(0, py, fb_info.width, PANEL_HEIGHT, DESKTOP_PANEL_BG);
}

void desktop_draw_start_button(void) {
    uint32_t py = fb_info.height - PANEL_HEIGHT;
    uint32_t bx = 4;
    uint32_t by = py + 4;
    fb_rect_fill(bx, by, PANEL_BUTTON_W - 8, PANEL_BUTTON_H - 8, 0x0F1F2F);
    uint32_t bird_x = bx + (PANEL_BUTTON_W - 8 - 24) / 2;
    uint32_t bird_y = by + (PANEL_BUTTON_H - 8 - 24) / 2;
    draw_bird_24x24(bird_x, bird_y);
}

void desktop_draw_clock(void) {
    rtc_time_t t = rtc_get_time();
    char buf[6];
    buf[0] = '0' + (t.hour / 10);
    buf[1] = '0' + (t.hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (t.min / 10);
    buf[4] = '0' + (t.min % 10);
    buf[5] = 0;
    uint32_t py = fb_info.height - PANEL_HEIGHT;
    uint32_t tw = text_width(buf);
    uint32_t tx = fb_info.width - tw - 12;
    uint32_t ty = py + (PANEL_HEIGHT - 16) / 2;
    fb_rect_fill(tx - 6, py + 4, tw + 12, PANEL_HEIGHT - 8, 0x0F1F2F);
    fb_print(buf, tx, ty, 0xFFFFFF, 0x0F1F2F);
}

/* ============================================
   Отрисовка ярлыков в буфер
   ============================================ */
void desktop_icons_draw_to_buffer(uint32_t* buf, int bw, int bh) {
    for (int i = 0; i < desktop_icons_count; i++) {
        DesktopIcon* ic = &desktop_icons[i];
        if (ic->fs_index < 0) continue;

        FsObject* o = fs_get(ic->fs_index);
        if (!o || o->type == OBJ_FREE) continue;

        const uint32_t (*img)[32] = 0;

        /* Ярлык — иконка по типу цели */
        if (ends_with(o->name, ".yrl")) {
            FsObject* t = fs_get(o->target_id);
            if (t && t->type != OBJ_FREE) {
                switch (t->type) {
                    case OBJ_APP_TERMINAL: img = icon_terminal; break;
                    case OBJ_APP_TRASH:    img = icon_trash;    break;
                    case OBJ_APP_NANO:     img = icon_nano;     break;
                    case OBJ_APP_FILER:    img = icon_filer;    break;
                    case OBJ_DIR:          img = icon_folder;   break;
                    case OBJ_FILE:         img = icon_file;     break;
                }
            }
        }
        /* Обычные объекты — иконка по ТИПУ (а не по имени) */
        else if (o->type == OBJ_APP_TERMINAL) img = icon_terminal;
        else if (o->type == OBJ_APP_NANO)     img = icon_nano;
        else if (o->type == OBJ_APP_FILER)    img = icon_filer;
        else if (o->type == OBJ_APP_TRASH)    img = icon_trash;
        else if (o->type == OBJ_DIR)          img = icon_folder;
        else if (o->type == OBJ_FILE)         img = icon_file;

        if (!img) continue;

        draw_image_32x32_buf(buf, bw, bh, img,
                             ic->x + DESKTOP_ICON_PAD,
                             ic->y);

        /* Подпись: убираем .yrl */
        char label_buf[FS_NAME_LEN];
        const char* label = o->name;
        if (ends_with(o->name, ".yrl")) {
            int n = strlen(o->name) - 4;
            if (n > FS_NAME_LEN - 1) n = FS_NAME_LEN - 1;
            for (int k = 0; k < n; k++) label_buf[k] = o->name[k];
            label_buf[n] = 0;
            label = label_buf;
        }

        char short_label[16];
        int max_chars = 10;
        int len = strlen(label);
        if (len > max_chars) {
            for (int k = 0; k < max_chars - 3; k++) short_label[k] = label[k];
            short_label[max_chars-3] = '.';
            short_label[max_chars-2] = '.';
            short_label[max_chars-1] = '.';
            short_label[max_chars] = 0;
            label = short_label;
        }

        int tw = strlen(label) * 8;
        int tx = ic->x + (DESKTOP_ICON_SIZE - tw) / 2;
        int ty = ic->y + DESKTOP_ICON_IMG + 4;
        fb_print_to_buffer(buf, bw, bh, label, tx, ty,
                           0xFFFFFF, DESKTOP_BG);
    }
}

/* ============================================
   Перетаскивание и клик
   ============================================ */
int desktop_icons_update(int mx, int my, int left_down, int left_prev) {
    int clicked = -1;

    for (int i = 0; i < desktop_icons_count; i++) {
        if (!desktop_icons[i].dragging) continue;

        if (!left_down) {
            if (!desktop_icons[i].moved) clicked = i;
            desktop_icons[i].dragging = 0;
            desktop_icons[i].moved = 0;
            return clicked;
        }

        int nx = mx - desktop_icons[i].drag_dx;
        int ny = my - desktop_icons[i].drag_dy;

        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx + DESKTOP_ICON_SIZE > (int)fb_info.width)
            nx = (int)fb_info.width - DESKTOP_ICON_SIZE;
        if (ny + DESKTOP_ICON_IMG + DESKTOP_ICON_TEXT > (int)fb_info.height - PANEL_HEIGHT)
            ny = (int)fb_info.height - PANEL_HEIGHT - DESKTOP_ICON_IMG - DESKTOP_ICON_TEXT;

        if (nx != desktop_icons[i].x || ny != desktop_icons[i].y) {
            desktop_icons[i].x = nx;
            desktop_icons[i].y = ny;
            desktop_icons[i].moved = 1;
        }
        return -1;
    }

    int just_pressed = left_down && !left_prev;
    if (just_pressed) {
        for (int i = desktop_icons_count - 1; i >= 0; i--) {
            int rx, ry, rw, rh;
            icon_rect(&desktop_icons[i], &rx, &ry, &rw, &rh);
            if (mx >= rx && mx < rx + rw && my >= ry && my < ry + rh) {
                desktop_icons[i].dragging = 1;
                desktop_icons[i].drag_dx = mx - desktop_icons[i].x;
                desktop_icons[i].drag_dy = my - desktop_icons[i].y;
                desktop_icons[i].moved = 0;
                return -1;
            }
        }
    }

    return -1;
}

/* ============================================
   Курсор
   ============================================ */
uint32_t cursor_backup[CURSOR_H][CURSOR_W];
int      cursor_backup_x = 0;
int      cursor_backup_y = 0;
int      cursor_backup_valid = 0;

void desktop_cursor_save(int x, int y) {
    if (!fb_enabled) return;
    if (x < 0 || y < 0 ||
        x + CURSOR_W > (int)fb_info.width ||
        y + CURSOR_H > (int)fb_info.height) {
        cursor_backup_valid = 0;
        return;
    }
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
            cursor_backup[row][col] = fb_getpixel(x + col, y + row);
    cursor_backup_x = x;
    cursor_backup_y = y;
    cursor_backup_valid = 1;
}

void desktop_cursor_restore(void) {
    if (!fb_enabled || !cursor_backup_valid) return;
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++) {
            int px = cursor_backup_x + col;
            int py = cursor_backup_y + row;
            if (px >= 0 && px < (int)fb_info.width &&
                py >= 0 && py < (int)fb_info.height)
                fb_putpixel(px, py, cursor_backup[row][col]);
        }
    cursor_backup_valid = 0;
}

void desktop_draw_cursor(uint32_t x, uint32_t y) {
    static const uint8_t cur[12][8] = {
        {1,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0},{1,2,1,0,0,0,0,0},
        {1,2,2,1,0,0,0,0},{1,2,2,2,1,0,0,0},{1,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,1,0},{1,2,2,2,2,2,2,1},{1,2,2,2,2,2,1,0},
        {1,2,2,1,1,2,2,1},{1,2,1,0,0,1,2,2},{1,1,0,0,0,0,1,1}
    };
    for (int row = 0; row < 12; row++)
        for (int col = 0; col < 8; col++) {
            uint8_t v = cur[row][col];
            int px = x + col;
            int py = y + row;
            if (px < 0 || px >= (int)fb_info.width) continue;
            if (py < 0 || py >= (int)fb_info.height) continue;
            if (v == 1)      fb_putpixel(px, py, 0x000000);
            else if (v == 2) fb_putpixel(px, py, 0xFFFFFF);
        }
}

void desktop_draw(void) {
    if (!fb_enabled) return;
    desktop_draw_background();
    desktop_draw_panel();
    desktop_draw_start_button();
    desktop_draw_clock();
}