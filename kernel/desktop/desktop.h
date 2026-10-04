#ifndef DESKTOP_H
#define DESKTOP_H

#include "../vga/vga.h"
#include "../framebuffer/framebuffer.h"
#include "../fs/fs.h"
#include "../utils/utils.h"

#define DESKTOP_ICON_SIZE    48
#define DESKTOP_ICON_IMG     32
#define DESKTOP_ICON_PAD     8
#define DESKTOP_ICON_TEXT    16

#define PANEL_HEIGHT         48
#define PANEL_BUTTON_W       64
#define PANEL_BUTTON_H       48

#define DESKTOP_BG           0x2A4A7A
#define DESKTOP_PANEL_BG     0x1A2A3A
#define DESKTOP_ICON_TEXT_CLR 0xFFFFFF

#define CURSOR_W  8
#define CURSOR_H  12

#define DESKTOP_MAX_ICONS 24

typedef struct {
    int fs_index;
    int x, y;
    int dragging;
    int drag_dx, drag_dy;
    int moved;
} DesktopIcon;

extern DesktopIcon desktop_icons[DESKTOP_MAX_ICONS];
extern int         desktop_icons_count;

#define ICON_TERMINAL_X   16
#define ICON_TERMINAL_Y   16
#define ICON_TRASH_X      16
#define ICON_TRASH_Y      80
#define ICON_NANO_X       16
#define ICON_NANO_Y       144
#define ICON_FILER_X      16
#define ICON_FILER_Y      208

void desktop_icons_init(void);
void desktop_scan_files(void);
int  desktop_get_folder_index(void);
int  desktop_get_trash_index(void);
void desktop_icons_draw_to_buffer(uint32_t* buf, int bw, int bh);
int  desktop_icons_update(int mx, int my, int left_down, int left_prev);

extern uint32_t cursor_backup[CURSOR_H][CURSOR_W];
extern int      cursor_backup_x;
extern int      cursor_backup_y;
extern int      cursor_backup_valid;

void desktop_cursor_save(int x, int y);
void desktop_cursor_restore(void);
void desktop_draw_cursor(uint32_t x, uint32_t y);

void desktop_draw(void);
void desktop_draw_background(void);
void desktop_draw_panel(void);
void desktop_draw_start_button(void);
void desktop_draw_clock(void);

#endif