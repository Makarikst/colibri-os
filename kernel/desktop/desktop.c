#include "desktop.h"
#include "icons.h"
#include "bird.h"
#include "../fs/fs.h"
#include "../utils/utils.h"
#include "../settings/settings.h"
#include "../wm/wm.h"

DesktopIcon desktop_icons[DESKTOP_MAX_ICONS];
int         desktop_icons_count = 0;

/* ============================================
   Внутренние функции
   ============================================ */

static int find_user_folder(const char* name) {
    int alpha = parse_path("/colibri/users/alpha", ROOT_INDEX);
    if (alpha >= 0) {
        int d = fs_find_in(alpha, name);
        if (d >= 0) return d;
    }
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (o && o->type == OBJ_DIR && strcmp(o->name, name) == 0) return i;
    }
    return -1;
}

static int slot_busy(int x, int y, int count) {
    for (int i = 0; i < count; i++)
        if (desktop_icons[i].x == x && desktop_icons[i].y == y) return 1;
    return 0;
}

int find_icon_in_manifest(int fs_idx) {
    FsObject* o = fs_get(fs_idx);
    if (!o) return -1;
    const char* p = o->data;
    while (*p) {
        if (strncmp(p, "icon:", 5) == 0) {
            p += 5;
            while (*p == ' ' || *p == '\t' || *p == '"') p++;
            char path[256];
            int i = 0;
            while (*p && *p != '"' && *p != ';' && i < 255) path[i++] = *p++;
            path[i] = 0;
            return parse_path(path, ROOT_INDEX);
        }
        p++;
    }
    return -1;
}

void draw_png_from_file(uint32_t* buf, int bw, int bh, int fs_idx, int x, int y) {
    FsObject* file = fs_get(fs_idx);
    if (!file) return;

    /* Формат GraphTool: сырые 32x32 по 4 байта */
    if (file->size == 32 * 32 * 4) {
        for (int py = 0; py < 32; py++)
            for (int px = 0; px < 32; px++) {
                int k = (py * 32 + px) * 4;
                uint32_t c = ((uint32_t)(unsigned char)file->data[k])
                           | ((uint32_t)(unsigned char)file->data[k+1] << 8)
                           | ((uint32_t)(unsigned char)file->data[k+2] << 16);
                int fx = x + px, fy = y + py;
                if (fx >= 0 && fx < bw && fy >= 0 && fy < bh) buf[fy * bw + fx] = c;
            }
        return;
    }

    /* Формат GTIM */
    if (file->size < 6) return;
    if (file->data[0] != 'G' || file->data[1] != 'T' || file->data[2] != 'I' || file->data[3] != 'M') return;

    int cw = (unsigned char)file->data[4];
    int ch = (unsigned char)file->data[5];
    int pos = 6;
    for (int py = 0; py < ch; py++) {
        for (int px = 0; px < cw; px++) {
            if (pos + 2 >= file->size) return;
            uint32_t c = ((uint32_t)(unsigned char)file->data[pos] << 16) |
                         ((uint32_t)(unsigned char)file->data[pos+1] << 8) |
                          (uint32_t)(unsigned char)file->data[pos+2];
            pos += 3;
            if (px >= 32 || py >= 32) continue;
            int fx = x + px, fy = y + py;
            if (fx >= 0 && fx < bw && fy >= 0 && fy < bh) buf[fy * bw + fx] = c;
        }
    }
}

static void draw_image_32x32_buf(uint32_t* buf, int bw, int bh, const uint32_t img[32][32], int x, int y) {
    for (int row = 0; row < 32; row++)
        for (int col = 0; col < 32; col++) {
            uint32_t c = img[row][col];
            if (c == 0) continue;
            int px = x + col, py = y + row;
            if (px >= 0 && px < bw && py >= 0 && py < bh) buf[py * bw + px] = c;
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

/* ============================================
   Публичные функции
   ============================================ */

void desktop_icons_init(void) {
    for (int i = 0; i < DESKTOP_MAX_ICONS; i++) {
        desktop_icons[i].fs_index = -1;
        desktop_icons[i].x = 0; desktop_icons[i].y = 0;
        desktop_icons[i].dragging = 0;
        desktop_icons[i].drag_dx = 0; desktop_icons[i].drag_dy = 0;
        desktop_icons[i].moved = 0;
    }
    desktop_icons_count = 0;
}

int desktop_get_folder_index(void) { return find_user_folder("Desktop"); }
int desktop_get_trash_index(void)  { return find_user_folder("Trash"); }

void desktop_scan_files(void) {
    int old_count = desktop_icons_count;
    DesktopIcon old_icons[DESKTOP_MAX_ICONS];
    for (int i = 0; i < old_count; i++) old_icons[i] = desktop_icons[i];

    desktop_icons_count = 0;
    int desktop_idx = desktop_get_folder_index();
    if (desktop_idx < 0) return;

    int is_new[DESKTOP_MAX_ICONS];

    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type == OBJ_FREE) continue;
        if (o->parent != desktop_idx) continue;
        if (desktop_icons_count >= DESKTOP_MAX_ICONS) break;

        DesktopIcon* ic = &desktop_icons[desktop_icons_count];
        ic->fs_index = i;
        ic->dragging = 0;
        ic->moved = 0;
        is_new[desktop_icons_count] = 1;

        for (int j = 0; j < old_count; j++) {
            if (old_icons[j].fs_index == i) {
                ic->x = old_icons[j].x; ic->y = old_icons[j].y;
                is_new[desktop_icons_count] = 0; break;
            }
        }
        desktop_icons_count++;
    }

    for (int n = 0; n < desktop_icons_count; n++) {
        if (!is_new[n]) continue;
        int y = 16;
        while (slot_busy(ICON_TERMINAL_X, y, desktop_icons_count) &&
               y + DESKTOP_ICON_IMG + DESKTOP_ICON_TEXT < (int)fb_info.height - PANEL_HEIGHT)
            y += 64;
        desktop_icons[n].x = ICON_TERMINAL_X;
        desktop_icons[n].y = y;
    }
}

void desktop_place_icon(int fs_idx, int cx, int cy) {
    for (int i = 0; i < desktop_icons_count; i++) {
        if (desktop_icons[i].fs_index != fs_idx) continue;
        int nx = cx - DESKTOP_ICON_SIZE / 2;
        int ny = cy - DESKTOP_ICON_IMG / 2;
        int maxy = (int)fb_info.height - PANEL_HEIGHT - DESKTOP_ICON_IMG - DESKTOP_ICON_TEXT;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx + DESKTOP_ICON_SIZE > (int)fb_info.width) nx = (int)fb_info.width - DESKTOP_ICON_SIZE;
        if (ny > maxy) ny = maxy;
        desktop_icons[i].x = nx; desktop_icons[i].y = ny;
        return;
    }
}

void desktop_draw_background(void) { fb_clear(DESKTOP_BG); }

void desktop_draw_panel(void) {
    uint32_t py = fb_info.height - PANEL_HEIGHT;
    fb_rect_fill(0, py, fb_info.width, PANEL_HEIGHT, DESKTOP_PANEL_BG);
}

void desktop_draw_start_button(void) {
    uint32_t py = fb_info.height - PANEL_HEIGHT;
    uint32_t bx = 4; uint32_t by = py + 4;
    fb_rect_fill(bx, by, PANEL_BUTTON_W - 8, PANEL_BUTTON_H - 8, 0x0F1F2F);
    draw_bird_24x24(bx + (PANEL_BUTTON_W - 8 - 24) / 2, by + (PANEL_BUTTON_H - 8 - 24) / 2);
}

void desktop_draw_clock(void) {
    rtc_time_t t = rtc_get_time();
    char buf[6];
    buf[0] = '0' + (t.hour / 10); buf[1] = '0' + (t.hour % 10); buf[2] = ':';
    buf[3] = '0' + (t.min / 10); buf[4] = '0' + (t.min % 10); buf[5] = 0;
    uint32_t py = fb_info.height - PANEL_HEIGHT;
    uint32_t tw = text_width(buf);
    uint32_t tx = fb_info.width - tw - 12;
    uint32_t ty = py + (PANEL_HEIGHT - 16) / 2;
    fb_rect_fill(tx - 6, py + 4, tw + 12, PANEL_HEIGHT - 8, 0x0F1F2F);
    fb_print(buf, tx, ty, 0xFFFFFF, 0x0F1F2F);
}

/* Кнопка Workspaces рядом с часами */
void desktop_draw_ws_button(void) {
    uint32_t py = fb_info.height - PANEL_HEIGHT;
    uint32_t bx = fb_info.width - 140;
    uint32_t by = py + 4;
    uint32_t bw = 60;
    uint32_t bh = PANEL_HEIGHT - 8;

    fb_rect_fill(bx, by, bw, bh, 0x0F1F2F);
    fb_print("WS", bx + 8, py + 12, 0xFFFFFF, 0x0F1F2F);

    char num[4];
    int n = wm_current_workspace + 1;
    num[0] = '0' + (n / 10);
    num[1] = '0' + (n % 10);
    num[2] = 0;
    fb_print(num, bx + 40, py + 12, 0xCCCCCC, 0x0F1F2F);
}

void desktop_icons_draw_to_buffer(uint32_t* buf, int bw, int bh) {
    for (int i = 0; i < desktop_icons_count; i++) {
        DesktopIcon* ic = &desktop_icons[i];
        if (ic->fs_index < 0) continue;
        FsObject* o = fs_get(ic->fs_index);
        if (!o || o->type == OBJ_FREE) continue;

        int ix = ic->x + DESKTOP_ICON_PAD;
        int iy = ic->y;
        int drew_icon = 0;

        if (ends_with(o->name, ".yrl")) {
            int target = o->target_id;
            if (target >= 0) {
                FsObject* t = fs_get(target);
                if (t) {
                    if (starts_with(t->name, "Terminal")) { draw_image_32x32_buf(buf, bw, bh, icon_terminal, ix, iy); drew_icon = 1; }
                    else if (starts_with(t->name, "Filer")) { draw_image_32x32_buf(buf, bw, bh, icon_filer, ix, iy); drew_icon = 1; }
                    else if (starts_with(t->name, "Nano")) { draw_image_32x32_buf(buf, bw, bh, icon_nano, ix, iy); drew_icon = 1; }
                    else if (starts_with(t->name, "GraphTool")) { draw_image_32x32_buf(buf, bw, bh, icon_graphtool, ix, iy); drew_icon = 1; }
                    else if (starts_with(t->name, "Browser")) { draw_image_32x32_buf(buf, bw, bh, icon_browser, ix, iy); drew_icon = 1; }
                    else if (starts_with(t->name, "Settings")) { draw_image_32x32_buf(buf, bw, bh, icon_settings, ix, iy); drew_icon = 1; }
                    else if (starts_with(t->name, "Studio")) { draw_image_32x32_buf(buf, bw, bh, icon_studio, ix, iy); drew_icon = 1; }
                    else {
                        int png_idx = find_icon_in_manifest(target);
                        if (png_idx >= 0) { draw_png_from_file(buf, bw, bh, png_idx, ix, iy); drew_icon = 1; }
                    }
                }
            }
        }
        else if (ends_with(o->name, ".capp")) {
            if (starts_with(o->name, "Terminal")) { draw_image_32x32_buf(buf, bw, bh, icon_terminal, ix, iy); drew_icon = 1; }
            else if (starts_with(o->name, "Filer")) { draw_image_32x32_buf(buf, bw, bh, icon_filer, ix, iy); drew_icon = 1; }
            else if (starts_with(o->name, "Nano")) { draw_image_32x32_buf(buf, bw, bh, icon_nano, ix, iy); drew_icon = 1; }
            else if (starts_with(o->name, "GraphTool")) { draw_image_32x32_buf(buf, bw, bh, icon_graphtool, ix, iy); drew_icon = 1; }
            else if (starts_with(o->name, "Browser")) { draw_image_32x32_buf(buf, bw, bh, icon_browser, ix, iy); drew_icon = 1; }
            else if (starts_with(o->name, "Settings")) { draw_image_32x32_buf(buf, bw, bh, icon_settings, ix, iy); drew_icon = 1; }
            else if (starts_with(o->name, "Studio")) { draw_image_32x32_buf(buf, bw, bh, icon_studio, ix, iy); drew_icon = 1; }
            else {
                int png_idx = find_icon_in_manifest(ic->fs_index);
                if (png_idx >= 0) { draw_png_from_file(buf, bw, bh, png_idx, ix, iy); drew_icon = 1; }
            }
        }
        else if (ends_with(o->name, ".png") || ends_with(o->name, ".jpg")) {
            draw_image_32x32_buf(buf, bw, bh, icon_image, ix, iy);
            drew_icon = 1;
        }
        else if (ends_with(o->name, ".cai")) {
            draw_image_32x32_buf(buf, bw, bh, icon_cai, ix, iy); drew_icon = 1;
        }
        else if (o->type == OBJ_DIR) { draw_image_32x32_buf(buf, bw, bh, icon_folder, ix, iy); drew_icon = 1; }
        else if (o->type == OBJ_FILE) { draw_image_32x32_buf(buf, bw, bh, icon_file, ix, iy); drew_icon = 1; }

        if (!drew_icon) draw_image_32x32_buf(buf, bw, bh, icon_file, ix, iy);

        char label_buf[FS_NAME_LEN];
        const char* label = o->name;
        if (ends_with(o->name, ".yrl")) {
            int n = strlen(o->name) - 4;
            if (n > FS_NAME_LEN - 1) n = FS_NAME_LEN - 1;
            for (int k = 0; k < n; k++) label_buf[k] = o->name[k];
            label_buf[n] = 0; label = label_buf;
        } else if (ends_with(o->name, ".capp")) {
            int n = strlen(o->name) - 5;
            if (n > FS_NAME_LEN - 1) n = FS_NAME_LEN - 1;
            for (int k = 0; k < n; k++) label_buf[k] = o->name[k];
            label_buf[n] = 0; label = label_buf;
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
        fb_print_to_buffer(buf, bw, bh, label, tx, ty, 0xFFFFFF, DESKTOP_BG);
    }
}

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
        if (nx < 0) nx = 0; if (ny < 0) ny = 0;
        if (nx + DESKTOP_ICON_SIZE > (int)fb_info.width) nx = (int)fb_info.width - DESKTOP_ICON_SIZE;
        if (ny + DESKTOP_ICON_IMG + DESKTOP_ICON_TEXT > (int)fb_info.height - PANEL_HEIGHT) ny = (int)fb_info.height - PANEL_HEIGHT - DESKTOP_ICON_IMG - DESKTOP_ICON_TEXT;
        if (nx != desktop_icons[i].x || ny != desktop_icons[i].y) {
            desktop_icons[i].x = nx; desktop_icons[i].y = ny; desktop_icons[i].moved = 1;
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

uint32_t cursor_backup[CURSOR_H][CURSOR_W];
int cursor_backup_x = 0; int cursor_backup_y = 0; int cursor_backup_valid = 0;

void desktop_cursor_save(int x, int y) {
    if (!fb_enabled) return;
    if (x < 0 || y < 0 || x + CURSOR_W > (int)fb_info.width || y + CURSOR_H > (int)fb_info.height) { cursor_backup_valid = 0; return; }
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++) cursor_backup[row][col] = fb_getpixel(x + col, y + row);
    cursor_backup_x = x; cursor_backup_y = y; cursor_backup_valid = 1;
}

void desktop_cursor_restore(void) {
    if (!fb_enabled || !cursor_backup_valid) return;
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++) {
            int px = cursor_backup_x + col; int py = cursor_backup_y + row;
            if (px >= 0 && px < (int)fb_info.width && py >= 0 && py < (int)fb_info.height) fb_putpixel(px, py, cursor_backup[row][col]);
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
            uint8_t v = cur[row][col]; int px = x + col; int py = y + row;
            if (px < 0 || px >= (int)fb_info.width || py < 0 || py >= (int)fb_info.height) continue;
            if (v == 1) fb_putpixel(px, py, 0x000000); else if (v == 2) fb_putpixel(px, py, 0xFFFFFF);
        }
}

void desktop_draw(void) {
    if (!fb_enabled) return;
    desktop_draw_background();
    desktop_draw_panel();
    desktop_draw_start_button();
    desktop_draw_clock();
    desktop_draw_ws_button();
}