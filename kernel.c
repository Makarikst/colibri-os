/* ============================================
   Colibri OS v1.0 — kernel_main
   ============================================ */

#include "kernel/vga/vga.h"
#include "kernel/kbd/kbd.h"
#include "kernel/utils/utils.h"
#include "kernel/kmalloc/kmalloc.h"
#include "kernel/fs/fs.h"
#include "kernel/framebuffer/framebuffer.h"
#include "kernel/desktop/desktop.h"
#include "kernel/mouse/mouse.h"
#include "kernel/wm/wm.h"
#include "kernel/term/term.h"
#include "kernel/menupkm/menupkm.h"
#include "kernel/filer/filer.h"
#include "kernel/nano_desk/nano_desk.h"
#include "kernel/startmenu/startmenu.h"
#include "kernel/graphtool/graphtool.h"

static uint32_t frame_buffer[1024 * 768];
static uint32_t desktop_backup[1024 * 768];
static uint32_t cursor_save[12][8];

static ContextMenu g_menu;
static StartMenu   g_start_menu;

static inline void bput(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= 1024 || y >= 768) return;
    frame_buffer[y * 1024 + x] = color;
}

static void draw_cursor_in_buffer(int x, int y) {
    static const uint8_t cur[12][8] = {
        {1,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0},{1,2,1,0,0,0,0,0},
        {1,2,2,1,0,0,0,0},{1,2,2,2,1,0,0,0},{1,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,1,0},{1,2,2,2,2,2,2,1},{1,2,2,2,2,2,1,0},
        {1,2,2,1,1,2,2,1},{1,2,1,0,0,1,2,2},{1,1,0,0,0,0,1,1}
    };
    for (int row = 0; row < 12; row++)
        for (int col = 0; col < 8; col++) {
            uint8_t v = cur[row][col];
            if (v == 1) bput(x + col, y + row, 0x000000);
            else if (v == 2) bput(x + col, y + row, 0xFFFFFF);
        }
}

static void save_under_cursor(int x, int y) {
    for (int row = 0; row < 12; row++)
        for (int col = 0; col < 8; col++) {
            int px = x + col, py = y + row;
            if (px < 0 || px >= 1024 || py < 0 || py >= 768) {
                cursor_save[row][col] = 0;
                continue;
            }
            cursor_save[row][col] = frame_buffer[py * 1024 + px];
        }
}

static void restore_under_cursor(int x, int y) {
    for (int row = 0; row < 12; row++)
        for (int col = 0; col < 8; col++) {
            int px = x + col, py = y + row;
            if (px < 0 || px >= 1024 || py < 0 || py >= 768) continue;
            frame_buffer[py * 1024 + px] = cursor_save[row][col];
        }
}

static void flush_region(int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 1024) x1 = 1024;
    if (y1 > 768) y1 = 768;
    uint32_t* fb = (uint32_t*)(unsigned long)fb_info.addr;
    int pitch32 = fb_info.pitch / 4;
    int width = x1 - x0;
    for (int y = y0; y < y1; y++) {
        uint32_t* src = &frame_buffer[y * 1024 + x0];
        uint32_t* dst = &fb[y * pitch32 + x0];
        for (int i = 0; i < width; i++) dst[i] = src[i];
    }
}

static void flush_frame(void) { flush_region(0, 0, 1024, 768); }

/* ============================================
   Открытие окон
   ============================================ */
static void open_terminal(void) {
    if (term.open) return;
    int win = wm_create(150, 100, 600, 380, WM_TYPE_TERMINAL, "Terminal");
    if (win < 0) return;
    term_open(&term, win, 150, 100, 600, 380);
    term.buf = frame_buffer;
    term.buf_w = 1024;
    term.buf_h = 768;
}

static void open_filer(void) {
    if (filer.open) return;
    int win = wm_create(200, 150, 500, 400, WM_TYPE_FILER, "Filer");
    if (win < 0) return;
    filer_open(&filer, win, 200, 150, 500, 400);
    filer.buf = frame_buffer;
    filer.buf_w = 1024;
    filer.buf_h = 768;
}

static void open_nano(int fs_index) {
    if (nano.open) return;
    int win = wm_create(250, 200, 600, 420, WM_TYPE_NANO, "Nano");
    if (win < 0) return;
    if (fs_index < 0) nano_desk_open_empty(win, 250, 200, 600, 420);
    else              nano_desk_open_file(fs_index, win, 250, 200, 600, 420);
    nano.buf_pixels = frame_buffer;
    nano.bw = 1024;
    nano.bh = 768;
}

static void open_graphtool(int fs_index) {
    if (graphtool.open) return;
    int win = wm_create(300, 100, 340, 420, WM_TYPE_GRAPHTOOL, "GraphTool");
    if (win < 0) return;
    graphtool_open(win, 300, 100, 340, 420, fs_index);
    graphtool.buf = frame_buffer;
    graphtool.buf_w = 1024;
    graphtool.buf_h = 768;
}

static void open_filer_at(int dir_index) {
    open_filer();
    filer.current_dir = dir_index;
    filer_reload(&filer);
}

/* ============================================
   Проверка, работает ли приложение (по магии CAPP)
   ============================================ */
/* ============================================
   Проверка, работает ли приложение (по магии CAPP)
   ============================================ */
static int app_is_working(const char* base_name) {
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
    if (apps < 0) return 0;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (!o || o->type == OBJ_FREE) continue;
        if (o->parent != apps) continue;
        if (!ends_with(o->name, ".capp")) continue;
        if (!starts_with(o->name, base_name)) continue;
        if (o->size >= 9 &&
            o->data[0] == 'C' && o->data[1] == 'A' &&
            o->data[2] == 'P' && o->data[3] == 'P') return 1;
    }
    return 0;
}

/* ============================================
   Открыть объект
   ============================================ */
static void open_object(FsObject* o, int fs_idx) {
    if (!o) return;

    /* Ярлык .yrl — идём к цели по target_id */
    if (ends_with(o->name, ".yrl")) {
        int target = o->target_id;
        if (target < 0 || target >= FS_MAX_OBJECTS) return;
        FsObject* t = fs_get(target);
        if (!t || t->type == OBJ_FREE) return;
        open_object(t, target);
        return;
    }

    /* .capp — только если приложение рабочее */
    if (ends_with(o->name, ".capp")) {
        if (app_is_working("Terminal") && starts_with(o->name, "Terminal"))
            open_terminal();
        else if (app_is_working("Filer") && starts_with(o->name, "Filer"))
            open_filer();
        else if (app_is_working("Nano")  && starts_with(o->name, "Nano"))
            open_nano(-1);
        else if (app_is_working("GraphTool") && starts_with(o->name, "GraphTool"))
            open_graphtool(-1);
        return;
    }

    /* Картинки .png / .jpg — в GraphTool, если он рабочий */
    if (ends_with(o->name, ".png") || ends_with(o->name, ".jpg")) {
        if (app_is_working("GraphTool"))
            open_graphtool(fs_idx);
        return;
    }

    /* Папка — только если Filer работает */
    if (o->type == OBJ_DIR) {
        if (app_is_working("Filer"))
            open_filer_at(fs_idx);
        return;
    }

    /* .txt / .nano — только если Nano работает */
    if (ends_with(o->name, ".txt") || ends_with(o->name, ".nano")) {
        if (app_is_working("Nano"))
            open_nano(fs_idx);
        return;
    }
}

static void window_content_cb(int type, int wx, int wy, int ww, int wh) {
    if (type == WM_TYPE_TERMINAL)  term_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_FILER) filer_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_NANO)  nano_desk_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_GRAPHTOOL) graphtool_render_for_window(wx, wy, ww, wh);
}

static int icon_hit_test(int mx, int my) {
    for (int i = desktop_icons_count - 1; i >= 0; i--) {
        if (desktop_icons[i].fs_index < 0) continue;
        int rx = desktop_icons[i].x;
        int ry = desktop_icons[i].y;
        int rw = DESKTOP_ICON_SIZE;
        int rh = DESKTOP_ICON_IMG + DESKTOP_ICON_TEXT;
        if (mx >= rx && mx < rx + rw && my >= ry && my < ry + rh) return i;
    }
    return -1;
}

static int point_in_window(int mx, int my) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        Window* w = wm_get(i);
        if (!w || !w->active) continue;
        if (mx >= w->x && mx < w->x + w->w &&
            my >= w->y && my < w->y + w->h) return 1;
    }
    return 0;
}

static void full_redraw(ContextMenu* menu, StartMenu* sm) {
    for (int i = 0; i < 1024 * 768; i++)
        frame_buffer[i] = desktop_backup[i];
    desktop_icons_draw_to_buffer(frame_buffer, 1024, 768);
    wm_draw_all(frame_buffer, 1024, 768, window_content_cb);
    menu_draw(menu, frame_buffer, 1024, 768);
    startmenu_draw(sm, frame_buffer, 1024, 768);
    save_under_cursor(mouse.x, mouse.y);
    draw_cursor_in_buffer(mouse.x, mouse.y);
    flush_frame();
}

static const char* menu_icon_labels[]  = { "Открыть", "Удалить", "Отмена" };
static const int   menu_icon_actions[] = { MENU_ACTION_OPEN, MENU_ACTION_DELETE, MENU_ACTION_CANCEL };
static const char* menu_desk_labels[]  = { "Создать файл", "Создать папку", "Отмена" };
static const int   menu_desk_actions[] = { MENU_ACTION_NEW_FILE, MENU_ACTION_NEW_FOLDER, MENU_ACTION_CANCEL };

void kernel_main(void) {
    vga_init();
    vga_banner();
    kbd_init();

    heap_init();
    fs_init();
    srand(12345);

    fb_init_bochs();
    mouse_init();
    wm_init();
    desktop_icons_init();

    if (!fb_enabled) while (1) cpu_halt();

    desktop_draw();
    {
        uint32_t* fb = (uint32_t*)(unsigned long)fb_info.addr;
        int pitch32 = fb_info.pitch / 4;
        for (int y = 0; y < (int)fb_info.height; y++)
            for (int x = 0; x < (int)fb_info.width; x++)
                desktop_backup[y * fb_info.width + x] = fb[y * pitch32 + x];
    }

    term_init(&term);
    open_terminal();
    filer_init(&filer);
    nano_desk_init();
    graphtool_init();

    desktop_scan_files();

    menu_init(&g_menu);
    startmenu_init(&g_start_menu);

    full_redraw(&g_menu, &g_start_menu);

    int prev_x = mouse.x, prev_y = mouse.y;
    int prev_left = mouse.left, prev_right = mouse.right;

    while (1) {
        mouse_poll();

        int left  = mouse.left;
        int right = mouse.right;
        int key_pressed = 0;
        int filer_changed = 0;
        int term_scrolled = 0;
        int menu_action_changed = 0;
        int startmenu_changed = 0;
        int graphtool_changed = 0;

        /* Меню Пуск */
        if (g_start_menu.open) {
            int sel = startmenu_update(&g_start_menu, mouse.x, mouse.y, left, prev_left);
            if (sel > 0) {
                int fs_idx = sel - 1;
                FsObject* o = fs_get(fs_idx);
                open_object(o, fs_idx);
                startmenu_changed = 1;
            } else if (!g_start_menu.open) {
                startmenu_changed = 1;
            }
        }

        int left_now = left && !prev_left;
        if (left_now && !g_start_menu.open && !g_menu.open) {
            int bx = 4;
            int by = fb_info.height - 48 + 4;
            int bw = 64 - 8;
            int bh = 48 - 8;
            if (mouse.x >= bx && mouse.x < bx + bw &&
                mouse.y >= by && mouse.y < by + bh) {
                startmenu_open(&g_start_menu, fb_info.width, fb_info.height);
                startmenu_changed = 1;
            }
        }

        /* Скролл терминала */
        if (mouse.wheel != 0 && term.open) {
            Window* tw = wm_get(term.win_idx);
            if (tw && tw->active &&
                mouse.x >= tw->x && mouse.x < tw->x + tw->w &&
                mouse.y >= tw->y && mouse.y < tw->y + tw->h) {
                term_scroll(&term, mouse.wheel);
                term_scrolled = 1;
            }
            mouse.wheel = 0;
        }

        /* Графический редактор — рисование */
        if (graphtool.open && !g_menu.open && !g_start_menu.open) {
            Window* gw = wm_get(graphtool.win_idx);
            if (gw && gw->active &&
                mouse.x >= gw->x && mouse.x < gw->x + gw->w &&
                mouse.y >= gw->y && mouse.y < gw->y + gw->h) {
                if (graphtool_handle_click(mouse.x, mouse.y, left, prev_left))
                    graphtool_changed = 1;
            }
        }

        /* Клавиатура */
        while (kbd_has_key()) {
            char k = kbd_get_key();
            if (k == 0) continue;
            key_pressed = 1;

            if (filer.open && filer.rename_mode) {
                if (filer_handle_key(&filer, k)) {
                    filer_changed = 1;
                    if (!filer.rename_mode) desktop_scan_files();
                    continue;
                }
            }

            if (filer.open && !filer.rename_mode && k == '\n') {
                int act = filer_handle_enter(&filer);
                if (act == 1) filer_changed = 1;
                else if (act == 2) {
                    int idx = filer.item_indices[filer.selected];
                    open_nano(idx);
                    filer_changed = 1;
                } else if (act == 3) filer_changed = 1;
                continue;
            }

            if (nano.open) nano_desk_handle_key(k);
            else if (term.open) {
                if (k == KEY_UP)   { term_scroll(&term, +1); term_scrolled = 1; }
                else if (k == KEY_DOWN) { term_scroll(&term, -1); term_scrolled = 1; }
                else term_handle_key(&term, k);
            }
        }

        int right_now = right && !prev_right;

        if (right_now && !g_menu.open && filer.open &&
            point_in_window(mouse.x, mouse.y)) {
            int mx = mouse.x, my = mouse.y;
            if (mx >= filer.cur_x && mx < filer.cur_x + filer.width &&
                my >= filer.cur_y && my < filer.cur_y + filer.height) {
                int row = (my - filer.cur_y) / FILER_ITEM_H;
                if (row >= 0 && row < filer.item_count) {
                    filer.selected = row;
                    filer_start_rename(&filer);
                    filer_changed = 1;
                }
            }
            prev_x = mouse.x; prev_y = mouse.y;
            prev_left = left; prev_right = right;
            full_redraw(&g_menu, &g_start_menu);
            continue;
        }

        if (right_now && !g_menu.open && !point_in_window(mouse.x, mouse.y)) {
            int hit = icon_hit_test(mouse.x, mouse.y);
            if (hit >= 0) menu_open(&g_menu, mouse.x, mouse.y, menu_icon_labels, menu_icon_actions, 3, hit);
            else          menu_open(&g_menu, mouse.x, mouse.y, menu_desk_labels, menu_desk_actions, 3, -1);
            full_redraw(&g_menu, &g_start_menu);
            prev_x = mouse.x; prev_y = mouse.y;
            prev_left = left; prev_right = right;
            continue;
        }

        if (g_menu.open) {
            int saved_target = g_menu.target_icon;
            int action = menu_update(&g_menu, mouse.x, mouse.y, left, prev_left);
            if (action != MENU_ACTION_NONE) {
                menu_action_changed = 1;
                if (action == MENU_ACTION_NEW_FILE) {
                    int d = desktop_get_folder_index();
                    if (d >= 0) fs_create_in(d, "new_file.txt", OBJ_FILE);
                    desktop_scan_files();
                } else if (action == MENU_ACTION_NEW_FOLDER) {
                    int d = desktop_get_folder_index();
                    if (d >= 0) fs_create_in(d, "new_folder", OBJ_DIR);
                    desktop_scan_files();
                } else if (action == MENU_ACTION_OPEN && saved_target >= 0) {
                    int fs_idx = desktop_icons[saved_target].fs_index;
                    if (fs_idx >= 0) {
                        FsObject* o = fs_get(fs_idx);
                        open_object(o, fs_idx);
                    }
                } else if (action == MENU_ACTION_DELETE && saved_target >= 0) {
                    int fs_idx = desktop_icons[saved_target].fs_index;
                    if (fs_idx >= 0) fs_delete_by_index(fs_idx);
                    desktop_scan_files();
                }
            }
        }

        if (filer.open && left && !prev_left && !g_menu.open) {
            if (filer_handle_click(&filer, mouse.x, mouse.y)) filer_changed = 1;
        }

        int icon_action_changed = 0;
        if (!g_menu.open && !g_start_menu.open) {
            int clicked_icon = desktop_icons_update(mouse.x, mouse.y, left, prev_left);
            if (clicked_icon >= 0) {
                icon_action_changed = 1;
                int fs_idx = desktop_icons[clicked_icon].fs_index;
                if (fs_idx >= 0) {
                    FsObject* o = fs_get(fs_idx);
                    open_object(o, fs_idx);
                }
            }
            for (int i = 0; i < desktop_icons_count; i++)
                if (desktop_icons[i].dragging) { icon_action_changed = 1; break; }
        }

        int wm_changed = 0;
        if (!g_menu.open && !g_start_menu.open) {
            wm_update(mouse.x, mouse.y, left, prev_left);
            for (int i = 0; i < WM_MAX_WINDOWS; i++) {
                Window* w = wm_get(i);
                if (w && w->active && w->dragging) { wm_changed = 1; break; }
            }
        }

        if (term.open && term.win_idx >= 0) {
            Window* w = wm_get(term.win_idx);
            if (!w || !w->active) { term_close(&term); wm_changed = 1; }
        }
        if (filer.open && filer.win_idx >= 0) {
            Window* w = wm_get(filer.win_idx);
            if (!w || !w->active) { filer_close(&filer); wm_changed = 1; }
        }
        if (nano.open && nano.win_idx >= 0) {
            Window* w = wm_get(nano.win_idx);
            if (!w || !w->active) { nano_desk_close(); wm_changed = 1; }
        }
        if (graphtool.open && graphtool.win_idx >= 0) {
            Window* w = wm_get(graphtool.win_idx);
            if (!w || !w->active) { graphtool_save(); graphtool_close(); wm_changed = 1; }
        }

        int mouse_moved     = (mouse.x != prev_x || mouse.y != prev_y);
        int buttons_changed = (left != prev_left || right != prev_right);

        int need_full = (key_pressed || menu_action_changed ||
                         icon_action_changed || wm_changed || filer_changed ||
                         term_scrolled || startmenu_changed || graphtool_changed);

        if (need_full) {
            full_redraw(&g_menu, &g_start_menu);
        } else if (mouse_moved || buttons_changed) {
            restore_under_cursor(prev_x, prev_y);
            save_under_cursor(mouse.x, mouse.y);
            draw_cursor_in_buffer(mouse.x, mouse.y);
            int x0 = (prev_x < mouse.x) ? prev_x : mouse.x;
            int y0 = (prev_y < mouse.y) ? prev_y : mouse.y;
            int x1 = ((prev_x > mouse.x) ? prev_x : mouse.x) + 8;
            int y1 = ((prev_y > mouse.y) ? prev_y : mouse.y) + 12;
            flush_region(x0, y0, x1, y1);
        }

        prev_x = mouse.x; prev_y = mouse.y;
        prev_left = left; prev_right = right;
    }
}