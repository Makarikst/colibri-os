#include "startmenu.h"
#include "../desktop/icons.h"
#include "../fs/fs.h"
#include "../utils/utils.h"
#include "../vga/vga.h"
#include "../i18n/i18n.h"
#include "../framebuffer/font8x16.h"

/* Из desktop.c (перенесите в desktop.h): */
int  find_icon_in_manifest(int fs_idx);
void draw_png_from_file(uint32_t* buf, int bw, int bh, int fs_idx, int x, int y);

void startmenu_init(StartMenu* m) {
    m->open = 0; m->x = 0; m->y = 0; m->w = STARTMENU_WIDTH; m->h = 0; m->count = 0; m->hover = -1;
    m->dragging = 0; m->drag_fs_index = -1; m->just_opened = 0;
}

void startmenu_open(StartMenu* m, int screen_w, int screen_h) {
    m->count = 0;
    int colibri = -1, users = -1, alpha = -1, apps = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == ROOT_INDEX && strcmp(o->name, "colibri") == 0) colibri = i;
    }
    for (int i = 0; i < FS_MAX_OBJECTS && colibri >= 0; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == colibri && strcmp(o->name, "users") == 0) users = i;
    }
    for (int i = 0; i < FS_MAX_OBJECTS && users >= 0; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == users && strcmp(o->name, "alpha") == 0) alpha = i;
    }
    for (int i = 0; i < FS_MAX_OBJECTS && alpha >= 0; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type != OBJ_DIR) continue;
        if (o->parent == alpha && strcmp(o->name, "Applications") == 0) apps = i;
    }
    if (apps < 0) {   // запасной вариант: любая папка Applications
        for (int i = 0; i < FS_MAX_OBJECTS; i++) {
            FsObject* o = fs_get(i);
            if (o && o->type == OBJ_DIR && strcmp(o->name, "Applications") == 0) { apps = i; break; }
        }
    }
    if (apps >= 0) {
        for (int i = 0; i < FS_MAX_OBJECTS; i++) {
            FsObject* o = fs_get(i);
            if (!o || o->type == OBJ_FREE) continue;
            if (o->parent != apps) continue;
            if (!ends_with(o->name, ".capp")) continue;
            if (m->count >= STARTMENU_MAX_ITEMS) break;
            m->fs_indices[m->count++] = i;
        }
    }
    m->h = m->count * STARTMENU_ITEM_H + 8;
    m->x = 4; m->y = screen_h - 48 - m->h; m->open = 1; m->hover = -1; m->dragging = 0; m->just_opened = 1;
}

void startmenu_close(StartMenu* m) { m->open = 0; m->count = 0; m->hover = -1; m->dragging = 0; m->just_opened = 0; }

int startmenu_update(StartMenu* m, int mx, int my, int left_down, int left_prev) {
    if (!m->open) return 0;
    if (m->just_opened) { if (!left_down) m->just_opened = 0; return 0; }

    int inside = (mx >= m->x && mx < m->x + m->w && my >= m->y && my < m->y + m->h);
    int row = -1;
    if (inside) { int ry = my - m->y - 4; row = (ry >= 0) ? ry / STARTMENU_ITEM_H : -1; if (row >= 0 && row < m->count) m->hover = row; else { m->hover = -1; row = -1; } }
    else m->hover = -1;

    int just_pressed = left_down && !left_prev;
    int just_released = !left_down && left_prev;

    if (just_pressed && !inside) { startmenu_close(m); return 0; }

    if (just_pressed && inside && row >= 0) {
        m->dragging = 1;
        m->drag_fs_index = m->fs_indices[row];
        return 0;
    }

    if (m->dragging && left_down) {
        if (mx > m->x + m->w + 20 || my < m->y - 20) {
            return 999;
        }
        return 0;
    }

    if (just_released) {
        if (m->dragging) {
            m->dragging = 0;
            if (inside && row >= 0) { int fs_idx = m->fs_indices[row]; startmenu_close(m); return fs_idx + 1; }
        } else if (inside && row >= 0) { int fs_idx = m->fs_indices[row]; startmenu_close(m); return fs_idx + 1; }
        else if (!inside) startmenu_close(m);
    }
    return 0;
}

void startmenu_draw(StartMenu* m, uint32_t* buf, int bw, int bh) {
    if (!m->open) return;
    for (int y = m->y; y < m->y + m->h; y++)
        for (int x = m->x; x < m->x + m->w; x++)
            if (x >= 0 && x < bw && y >= 0 && y < bh) buf[y * bw + x] = 0x202840;

    for (int x = m->x; x < m->x + m->w; x++) {
        if (x >= 0 && x < bw && m->y >= 0 && m->y < bh) buf[m->y * bw + x] = 0x4A6BC0;
        if (x >= 0 && x < bw && m->y + m->h - 1 >= 0 && m->y + m->h - 1 < bh) buf[(m->y + m->h - 1) * bw + x] = 0x4A6BC0;
    }

    for (int i = 0; i < m->count; i++) {
        int iy = m->y + 4 + i * STARTMENU_ITEM_H;
        FsObject* o = fs_get(m->fs_indices[i]);
        if (!o) continue;

        uint32_t bg = (i == m->hover) ? 0x3050C0 : 0x202840;
        for (int yy = iy; yy < iy + STARTMENU_ITEM_H - 4; yy++)
            for (int xx = m->x + 2; xx < m->x + m->w - 2; xx++)
                if (yy >= 0 && yy < bh && xx >= 0 && xx < bw) buf[yy * bw + xx] = bg;

        const uint32_t (*img)[32] = 0;
        if (ends_with(o->name, ".capp")) {
            if (starts_with(o->name, "Terminal"))     img = icon_terminal;
            else if (starts_with(o->name, "Filer"))   img = icon_filer;
            else if (starts_with(o->name, "Nano"))    img = icon_nano;
            else if (starts_with(o->name, "GraphTool")) img = icon_graphtool;
            else if (starts_with(o->name, "Browser")) img = icon_browser;
            else if (starts_with(o->name, "Settings")) img = icon_settings;
            else if (starts_with(o->name, "Studio")) img = icon_studio;
        }
        if (img) {
            int icon_x = m->x + 8; int icon_y = iy + 4;
            for (int row = 0; row < 32; row++)
                for (int col = 0; col < 32; col++) {
                    uint32_t c = img[row][col];
                    if (c == 0) continue;
                    int px = icon_x + col, py = icon_y + row;
                    if (px >= 0 && px < bw && py >= 0 && py < bh) buf[py * bw + px] = c;
                }
        } else if (ends_with(o->name, ".capp")) {
            // кастомное приложение: иконка из манифеста
            int png = find_icon_in_manifest(m->fs_indices[i]);
            if (png >= 0) draw_png_from_file(buf, bw, bh, png, m->x + 8, iy + 4);
        }

        char name_buf[FS_NAME_LEN];
        const char* display_name = o->name;
        if (starts_with(o->name, "Studio")) display_name = "Colibri Studio";
        else if (starts_with(o->name, "Terminal")) display_name = _("terminal");
        else if (starts_with(o->name, "Filer")) display_name = _("filer");
        else if (starts_with(o->name, "Nano")) display_name = _("nano");
        else if (starts_with(o->name, "Browser")) display_name = _("browser");
        else if (starts_with(o->name, "Settings")) display_name = _("settings");
        else if (starts_with(o->name, "GraphTool")) display_name = _("graphtool");
        else {
            int n = 0;
            while (o->name[n] && o->name[n] != '.' && n < FS_NAME_LEN - 1) { name_buf[n] = o->name[n]; n++; }
            name_buf[n] = 0; display_name = name_buf;
        }

        int tx = m->x + 50; int ty = iy + 16;
        const char* s = display_name;
        while (*s) {
            uint32_t cp = utf8_next(&s);
            const unsigned char* glyph = font_glyph(cp);
            for (int row = 0; row < 16; row++) {
                unsigned char bits = glyph[row];
                for (int col = 0; col < 8; col++) {
                    uint32_t color = (bits & (0x80 >> col)) ? 0xFFFFFF : bg;
                    int px = tx + col, py = ty + row;
                    if (px >= 0 && px < bw && py >= 0 && py < bh) buf[py * bw + px] = color;
                }
            }
            tx += 8;
        }
    }
}