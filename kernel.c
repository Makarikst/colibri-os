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

static uint32_t frame_buffer[1024 * 768];
static uint32_t desktop_backup[1024 * 768];
static uint32_t cursor_save[12][8];

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

    if (fs_index < 0)
        nano_desk_open_empty(win, 250, 200, 600, 420);
    else
        nano_desk_open_file(fs_index, win, 250, 200, 600, 420);

    nano.buf_pixels = frame_buffer;
    nano.bw = 1024;
    nano.bh = 768;
}

static void open_filer_at(int dir_index) {
    open_filer();
    filer.current_dir = dir_index;
    filer_reload(&filer);
}

/* ============================================
   Интерпретатор байт-кода .capp
   ============================================ */
static int run_capp(FsObject* o) {
    if (!o) return 0;

    if (o->data[0] != 'C' || o->data[1] != 'A' ||
        o->data[2] != 'P' || o->data[3] != 'P') return 0;
    if (o->data[4] != 2) return 0;

    int code_offset = (unsigned char)o->data[5] | ((unsigned char)o->data[6] << 8);
    int code_size   = (unsigned char)o->data[7] | ((unsigned char)o->data[8] << 8);

    if (code_size <= 0 || code_size > 4000) return 0;
    if (code_offset + code_size > o->size)  return 0;

    int pc = 0;
    while (pc < code_size) {
        unsigned char op = (unsigned char)o->data[code_offset + pc];
        pc++;

        if (op == 0xFF) break;

        switch (op) {
            case 0x01: {
                if (pc >= code_size) return 1;
                char c = o->data[code_offset + pc++];
                vga_putchar(c);
                break;
            }
            case 0x05: vga_putchar('\n'); break;
            case 0x10: open_terminal(); break;
            case 0x11: open_nano(-1);   break;
            case 0x12: open_filer();    break;
            default: return 1;
        }
    }
    return 1;
}

/* ============================================
   Открыть объект — по расширению
   ============================================ */
static void open_object(FsObject* o, int fs_idx) {
    if (!o) return;

    if (ends_with(o->name, ".yrl")) {
        int target = o->target_id;
        if (target < 0 || target >= FS_MAX_OBJECTS) return;
        FsObject* t = fs_get(target);
        if (!t || t->type == OBJ_FREE) return;
        open_object(t, target);
        return;
    }

    if (ends_with(o->name, ".capp")) {
        run_capp(o);
        return;
    }

    if (o->type == OBJ_DIR) {
        open_filer_at(fs_idx);
        return;
    }

    if (ends_with(o->name, ".txt") || ends_with(o->name, ".nano")) {
        open_nano(fs_idx);
        return;
    }
}

static void window_content_cb(int type, int wx, int wy, int ww, int wh) {
    if (type == WM_TYPE_TERMINAL) {
        term_render_for_window(wx, wy, ww, wh);
    } else if (type == WM_TYPE_FILER) {
        filer_render_for_window(wx, wy, ww, wh);
    } else if (type == WM_TYPE_NANO) {
        nano_desk_render_for_window(wx, wy, ww, wh);
    }
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
static const int   menu_icon_actions[] = { MENU_ACTION_OPEN,
                                           MENU_ACTION_DELETE,
                                           MENU_ACTION_CANCEL };

static const char* menu_desk_labels[]  = { "Создать файл", "Создать папку", "Отмена" };
static const int   menu_desk_actions[] = { MENU_ACTION_NEW_FILE,
                                           MENU_ACTION_NEW_FOLDER,
                                           MENU_ACTION_CANCEL };

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

    desktop_scan_files();

    ContextMenu menu;
    menu_init(&menu);

    StartMenu start_menu;
    startmenu_init(&start_menu);

    full_redraw(&menu, &start_menu);

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

        /* ============================================
           Меню Пуск
           ============================================ */
        if (start_menu.open) {
            int sel = startmenu_update(&start_menu, mouse.x, mouse.y, left, prev_left);
            if (sel > 0) {
                int fs_idx = sel - 1;
                FsObject* o = fs_get(fs_idx);
                open_object(o, fs_idx);
                startmenu_changed = 1;
            } else if (!start_menu.open) {
                startmenu_changed = 1;
            }
        }

        /* Клик по кнопке RUN (колибри) */
        int left_now = left && !prev_left;
        if (left_now && !start_menu.open && !menu.open) {
            int bx = 4;
            int by = fb_info.height - 48 + 4;
            int bw = 64 - 8;
            int bh = 48 - 8;
            if (mouse.x >= bx && mouse.x < bx + bw &&
                mouse.y >= by && mouse.y < by + bh) {
                startmenu_open(&start_menu, fb_info.width, fb_info.height);
                startmenu_changed = 1;
            }
        }

        /* ============================================
           Скролл терминала колёсиком
           ============================================ */
        if (mouse.wheel != 0 && term.open) {
            Window* tw = wm_get(term.win_idx);
            if (tw && tw->active &&
                mouse.x >= tw->x && mouse.x < tw->x + tw->w &&
                mouse.y >= tw->y && mouse.y < tw->y + tw->h)
            {
                term_scroll(&term, mouse.wheel);
                term_scrolled = 1;
            }
            mouse.wheel = 0;
        }

        /* ============================================
           Клавиатура
           ============================================ */
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
                if (act == 1) {
                    filer_changed = 1;
                } else if (act == 2) {
                    int idx = filer.item_indices[filer.selected];
                    open_nano(idx);
                    filer_changed = 1;
                } else if (act == 3) {
                    filer_changed = 1;
                }
                continue;
            }

            if (nano.open) {
                nano_desk_handle_key(k);
            }
            else if (term.open) {
                if (k == KEY_UP) {
                    term_scroll(&term, +1);
                    term_scrolled = 1;
                } else if (k == KEY_DOWN) {
                    term_scroll(&term, -1);
                    term_scrolled = 1;
                } else {
                    term_handle_key(&term, k);
                }
            }
        }

        int right_now = right && !prev_right;

        if (right_now && !menu.open && filer.open &&
            point_in_window(mouse.x, mouse.y))
        {
            int mx = mouse.x, my = mouse.y;
            if (mx >= filer.cur_x && mx < filer.cur_x + filer.width &&
                my >= filer.cur_y && my < filer.cur_y + filer.height)
            {
                int row = (my - filer.cur_y) / FILER_ITEM_H;
                if (row >= 0 && row < filer.item_count) {
                    filer.selected = row;
                    filer_start_rename(&filer);
                    filer_changed = 1;
                }
            }
            prev_x = mouse.x; prev_y = mouse.y;
            prev_left = left; prev_right = right;
            full_redraw(&menu, &start_menu);
            continue;
        }

        if (right_now && !menu.open && !point_in_window(mouse.x, mouse.y)) {
            int hit = icon_hit_test(mouse.x, mouse.y);
            if (hit >= 0)
                menu_open(&menu, mouse.x, mouse.y, menu_icon_labels, menu_icon_actions, 3, hit);
            else
                menu_open(&menu, mouse.x, mouse.y, menu_desk_labels, menu_desk_actions, 3, -1);
            full_redraw(&menu, &start_menu);
            prev_x = mouse.x; prev_y = mouse.y;
            prev_left = left; prev_right = right;
            continue;
        }

        if (menu.open) {
            int saved_target = menu.target_icon;
            int action = menu_update(&menu, mouse.x, mouse.y, left, prev_left);
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

        if (filer.open && left && !prev_left && !menu.open) {
            if (filer_handle_click(&filer, mouse.x, mouse.y)) {
                filer_changed = 1;
            }
        }

        int icon_action_changed = 0;
        if (!menu.open && !start_menu.open) {
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
        if (!menu.open && !start_menu.open) {
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
            if (!w || !w->active) {
                nano_desk_close();
                wm_changed = 1;
            }
        }

        int mouse_moved     = (mouse.x != prev_x || mouse.y != prev_y);
        int buttons_changed = (left != prev_left || right != prev_right);

        int need_full = (key_pressed || menu_action_changed ||
                         icon_action_changed || wm_changed || filer_changed ||
                         term_scrolled || startmenu_changed);

        if (need_full) {
            full_redraw(&menu, &start_menu);
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