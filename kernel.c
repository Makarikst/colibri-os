/* ============================================
   Colibri OS v2.0 — kernel_main (Workspaces)
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
#include "kernel/net/pci.h"
#include "kernel/net/e1000.h"
#include "kernel/net/net.h"
#include "kernel/browser/browser.h"
#include "kernel/settings/settings.h"
#include "kernel/i18n/i18n.h"
#include "kernel/studio/studio.h"
#include "kernel/cx/cx.h"
#include "kernel/capp/capp.h"
#include "kernel/mission/mission.h"

static uint32_t frame_buffer[1024 * 768];
static uint32_t desktop_backup[1024 * 768];
static uint32_t cursor_save[12][8];

static ContextMenu g_menu;
static StartMenu   g_start_menu;

static void on_settings_apply_cb(void) {
    desktop_draw();
    uint32_t* fb = (uint32_t*)(unsigned long)fb_info.addr;
    int pitch32 = fb_info.pitch / 4;
    for (int y = 0; y < (int)fb_info.height; y++) {
        for (int x = 0; x < (int)fb_info.width; x++) {
            desktop_backup[y * fb_info.width + x] = fb[y * pitch32 + x];
        }
    }
}

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
    term.buf = frame_buffer; term.buf_w = 1024; term.buf_h = 768;
}

static void open_filer(void) {
    if (filer.open) return;
    int win = wm_create(200, 150, 500, 400, WM_TYPE_FILER, "Filer");
    if (win < 0) return;
    filer_open(&filer, win, 200, 150, 500, 400);
    filer.buf = frame_buffer; filer.buf_w = 1024; filer.buf_h = 768;
}

static void open_nano(int fs_index) {
    if (nano.open) return;
    int win = wm_create(250, 200, 600, 420, WM_TYPE_NANO, "Nano");
    if (win < 0) return;
    if (fs_index < 0) nano_desk_open_empty(win, 250, 200, 600, 420);
    else              nano_desk_open_file(fs_index, win, 250, 200, 600, 420);
    nano.buf_pixels = frame_buffer; nano.bw = 1024; nano.bh = 768;
}

static void open_graphtool(int fs_index) {
    if (graphtool.open) return;
    int win = wm_create(300, 100, 400, 420, WM_TYPE_GRAPHTOOL, "GraphTool");
    if (win < 0) return;
    graphtool_open(win, 300, 100, 400, 420, fs_index);
    graphtool.buf = frame_buffer; graphtool.buf_w = 1024; graphtool.buf_h = 768;
}

static void open_browser(void) {
    if (browser.open) return;
    int win = wm_create(120, 80, 760, 580, WM_TYPE_BROWSER, "Browser");
    if (win < 0) return;
    browser.buf = frame_buffer; browser.buf_w = 1024; browser.buf_h = 768;
    browser_open(win, 120, 80, 760, 580);
}

static void open_settings(void) {
    if (settings_app.open) return;
    int win = wm_create(300, 200, 400, 350, WM_TYPE_SETTINGS, "Settings");
    if (win < 0) return;
    settings_open(win, 300, 200, 400, 350);
    settings_app.buf = frame_buffer; settings_app.buf_w = 1024; settings_app.buf_h = 768;
}

static void open_studio(void) {
    if (studio.open) return;
    int win = wm_create(100, 100, 700, 500, WM_TYPE_EMPTY, "Colibri Studio");
    if (win < 0) return;
    studio_open(win, 100, 100, 700, 500);
    studio.buf = frame_buffer; studio.buf_w = 1024; studio.buf_h = 768;
}

static void open_filer_at(int dir_index) {
    open_filer();
    filer.current_dir = dir_index;
    filer_reload(&filer);
}

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

    if (ends_with(o->name, ".cai")) {
        int apps_idx = parse_path("/colibri/users/alpha/Applications", ROOT_INDEX);
        if (apps_idx >= 0) {
            char new_name[32] = "MyApp";
            const char* p = o->data;
            while (*p) {
                if (strncmp(p, "label:", 6) == 0) {
                    p += 6;
                    while (*p == ' ' || *p == '"') p++;
                    int i = 0;
                    while (*p && *p != '"' && *p != ';' && i < 30) {
                        if (*p == ' ') new_name[i++] = '_';
                        else new_name[i++] = *p;
                        p++;
                    }
                    new_name[i] = 0;
                    break;
                }
                p++;
            }

            char file_name[64];
            int fi = 0; const char* an = new_name;
            while (*an) file_name[fi++] = *an++;
            file_name[fi++] = '.'; file_name[fi++] = 'c'; file_name[fi++] = 'a'; file_name[fi++] = 'p'; file_name[fi++] = 'p'; file_name[fi] = 0;

            int capp_idx = fs_find_in(apps_idx, file_name);
            if (capp_idx < 0) capp_idx = fs_create_in(apps_idx, file_name, OBJ_FILE);
            if (capp_idx >= 0) {
                FsObject* app = fs_get(capp_idx);
                int k = 0;
                while (o->data[k] && k < FS_DATA_LEN - 1) { app->data[k] = o->data[k]; k++; }
                app->data[k] = 0; app->size = k;
                vga_print("App installed: "); vga_print(file_name); vga_print("\n");
            }
        }
        return;
    }

    if (ends_with(o->name, ".capp")) {
        if (starts_with(o->name, "Terminal")) open_terminal();
        else if (starts_with(o->name, "Filer")) open_filer();
        else if (starts_with(o->name, "Nano"))  open_nano(-1);
        else if (starts_with(o->name, "GraphTool")) open_graphtool(-1);
        else if (starts_with(o->name, "Browser")) open_browser();
        else if (starts_with(o->name, "Settings")) open_settings();
        else if (starts_with(o->name, "Studio")) open_studio();
        else capp_open(fs_idx);
        return;
    }

    if (ends_with(o->name, ".png") || ends_with(o->name, ".jpg")) {
        open_graphtool(fs_idx);
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

void desktop_place_icon(int fs_idx, int cx, int cy);

static void create_desktop_link(int target_fs, int drop_x, int drop_y) {
    int desk_idx = desktop_get_folder_index();
    FsObject* app = fs_get(target_fs);
    if (desk_idx < 0 || !app) return;

    int link_idx = -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        FsObject* o = fs_get(i);
        if (o && o->type != OBJ_FREE && o->parent == desk_idx &&
            ends_with(o->name, ".yrl") && o->target_id == target_fs) { link_idx = i; break; }
    }
    if (link_idx < 0) {
        char link_name[40]; int i = 0;
        while (app->name[i] && app->name[i] != '.' && i < 30) { link_name[i] = app->name[i]; i++; }
        link_name[i++] = '.'; link_name[i++] = 'y'; link_name[i++] = 'r'; link_name[i++] = 'l'; link_name[i] = 0;
        link_idx = fs_create_in(desk_idx, link_name, OBJ_LINK);
        if (link_idx < 0) { vga_print("link create failed\n"); return; }
        fs_get(link_idx)->target_id = target_fs;
    }
    desktop_scan_files();
    if (drop_x >= 0) desktop_place_icon(link_idx, drop_x, drop_y);
}

static void window_content_cb(int type, int wx, int wy, int ww, int wh) {
    if (type == WM_TYPE_TERMINAL)  term_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_FILER) filer_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_NANO)  nano_desk_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_GRAPHTOOL) graphtool_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_BROWSER)   browser_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_SETTINGS)  settings_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_CAPP)      capp_render_for_window(wx, wy, ww, wh);
    else if (type == WM_TYPE_EMPTY && studio.open) studio_render_for_window(wx, wy, ww, wh);
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
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) return 1;
    }
    return 0;
}

static void full_redraw(ContextMenu* menu, StartMenu* sm) {
    for (int i = 0; i < 1024 * 768; i++) frame_buffer[i] = desktop_backup[i];
    desktop_icons_draw_to_buffer(frame_buffer, 1024, 768);
    wm_draw_all(frame_buffer, 1024, 768, window_content_cb);
    mission_render(frame_buffer, 1024, 768);
    menu_draw(menu, frame_buffer, 1024, 768);
    startmenu_draw(sm, frame_buffer, 1024, 768);
    save_under_cursor(mouse.x, mouse.y);
    draw_cursor_in_buffer(mouse.x, mouse.y);
    flush_frame();
}

static const char* menu_icon_labels[]  = { "Открыть", "Удалить", "Создать ярлык", "Отмена" };
static const int   menu_icon_actions[] = { MENU_ACTION_OPEN, MENU_ACTION_DELETE, MENU_ACTION_CREATE_LINK, MENU_ACTION_CANCEL };
static const char* menu_desk_labels[]  = { "Создать файл", "Создать папку", "Отмена" };
static const int   menu_desk_actions[] = { MENU_ACTION_NEW_FILE, MENU_ACTION_NEW_FOLDER, MENU_ACTION_CANCEL };

void kernel_main(void) {
    vga_init();
    vga_banner();
    kbd_init();

    heap_init();
    fs_init();
    srand(12345);

    i18n_init();
    i18n_set_lang(LANG_RU);
    pci_init();

    if (g_net_found) {
        uint32_t pci_cmd = pci_read32(g_net_device.bus, g_net_device.slot, g_net_device.func, 0x04);
        pci_cmd |= 0x06;
        pci_write32(g_net_device.bus, g_net_device.slot, g_net_device.func, 0x04, pci_cmd);
        if (e1000_init(g_net_device.bar0)) net_init();
        else vga_print_color("NET: E1000 init failed\n", VGA_LIGHT_RED);
    }

    vga_print("\n=== PRESS ANY KEY TO START GUI ===\n");
    while (!kbd_has_key()) { }
    kbd_get_key();

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

    term_init(&term); open_terminal();
    filer_init(&filer);
    nano_desk_init();
    graphtool_init();
    browser_init();
    settings_init();
    studio_init();
    capp_init();
    capp_set_buffer(frame_buffer, 1024, 768);

    settings_on_apply = on_settings_apply_cb;
    desktop_scan_files();

    menu_init(&g_menu);
    startmenu_init(&g_start_menu);
    full_redraw(&g_menu, &g_start_menu);

    int prev_x = mouse.x, prev_y = mouse.y;
    int prev_left = mouse.left, prev_right = mouse.right;

    while (1) {
        net_poll();
        if (!kbd_has_key()) mouse_poll();

        int left  = mouse.left;
        int right = mouse.right;
        int key_pressed = 0;
        int filer_changed = 0;
        int term_scrolled = 0;
        int menu_action_changed = 0;
        int startmenu_changed = 0;
        int graphtool_changed = 0;
        int browser_changed = 0;
        int settings_changed = 0;
        int studio_changed = 0;
        int wm_changed = 0;

        // МЕНЮ ПУСК
        if (g_start_menu.open) {
            int sel = startmenu_update(&g_start_menu, mouse.x, mouse.y, left, prev_left);
            if (sel == 999) {
                create_desktop_link(g_start_menu.drag_fs_index, mouse.x, mouse.y);
                startmenu_close(&g_start_menu);
                startmenu_changed = 1;
            } else if (sel > 0) {
                int fs_idx = sel - 1;
                FsObject* o = fs_get(fs_idx);
                open_object(o, fs_idx);
                startmenu_changed = 1;
            } else if (!g_start_menu.open) {
                startmenu_changed = 1;
            }
        }

        int left_now = left && !prev_left;

        // Кнопка Пуск + WS (Mission Control)
        if (left_now && !g_start_menu.open && !g_menu.open) {
            int bx = 4, by = fb_info.height - 48 + 4, bw = 64 - 8, bh = 48 - 8;
            if (mouse.x >= bx && mouse.x < bx + bw && mouse.y >= by && mouse.y < by + bh) {
                startmenu_open(&g_start_menu, fb_info.width, fb_info.height);
                startmenu_changed = 1;
            }
            int wx = fb_info.width - 140, wy = fb_info.height - 48 + 4;
            int ww = 60, wh = 48 - 8;
            if (mouse.x >= wx && mouse.x < wx + ww && mouse.y >= wy && mouse.y < wy + wh) {
                mission_toggle();
                wm_changed = 1;
                prev_x = mouse.x; prev_y = mouse.y;
                prev_left = left; prev_right = right;
                full_redraw(&g_menu, &g_start_menu);
                continue;   // <-- ВАЖНО: не отдавать этот клик mission_handle_click
            }
        }

        // Mission Control: клики — первыми
        if (mission_is_open()) {
            if (mission_handle_click(mouse.x, mouse.y, left, prev_left)) {
                wm_changed = 1;
                prev_x = mouse.x; prev_y = mouse.y;
                prev_left = left; prev_right = right;
                full_redraw(&g_menu, &g_start_menu);
                continue;
            }
        }

        // ОКНА
        if (mouse.wheel != 0 && term.open) {
            Window* tw = wm_get(term.win_idx);
            if (tw && tw->active && mouse.x >= tw->x && mouse.x < tw->x + tw->w && mouse.y >= tw->y && mouse.y < tw->y + tw->h) {
                term_scroll(&term, mouse.wheel); term_scrolled = 1;
            }
            mouse.wheel = 0;
        }

        if (graphtool.open) {
            Window* gw = wm_get(graphtool.win_idx);
            if (gw && gw->active) {
                if (graphtool_handle_click(mouse.x, mouse.y, left, prev_left)) graphtool_changed = 1;
                if (!left && prev_left) graphtool_changed = 1;
            }
        }

        if (studio.open) {
            Window* sw = wm_get(studio.win_idx);
            if (sw && sw->active) {
                if (studio_handle_click(mouse.x, mouse.y, left, prev_left)) studio_changed = 1;
            }
        }

        if (settings_app.open && !g_menu.open && !g_start_menu.open) {
            Window* sw = wm_get(settings_app.win_idx);
            if (sw && sw->active && mouse.x >= sw->x && mouse.x < sw->x + sw->w && mouse.y >= sw->y && mouse.y < sw->y + sw->h) {
                if (settings_handle_click(mouse.x, mouse.y, left, prev_left)) { settings_changed = 1; wm_changed = 1; }
            }
        }

        if (browser.open && !g_menu.open && !g_start_menu.open) {
            Window* bw = wm_get(browser.win_idx);
            if (bw && bw->active && mouse.x >= bw->x && mouse.x < bw->x + bw->w && mouse.y >= bw->y && mouse.y < bw->y + bw->h) {
                if (browser_handle_click(mouse.x, mouse.y, left, prev_left)) browser_changed = 1;
            }
        }

        while (kbd_has_key()) {
            char k = kbd_get_key();
            if (k == 0) continue;
            key_pressed = 1;

            // Mission Control: клавиши — первыми
            if (mission_is_open() && mission_handle_key(k)) { wm_changed = 1; continue; }

            if (k == KEY_SUPER_LEFT) {
                if (wm_current_workspace > 0) wm_current_workspace--;
                wm_changed = 1;
                continue;
            }
            if (k == KEY_SUPER_RIGHT) {
                if (wm_current_workspace < wm_workspace_count - 1) wm_current_workspace++;
                wm_changed = 1;
                continue;
            }

            // Глобальный выход через Ctrl+Q
            if (k == KEY_CTRL_Q) {
                int top = wm_zcount - 1;
                if (top >= 0) {
                    Window* w = wm_get(wm_zorder[top]);
                    if (w && w->active) {
                        // Если приложение было полноэкранным, возвращаем на Рабочий стол
                        if (w->is_fullscreen) {
                            wm_current_workspace = 0;
                            if (wm_workspace_count > 1) wm_workspace_count--;
                        }
                        // Закрываем окно (это вызовет закрытие самого приложения)
                        wm_close(wm_zorder[top]);
                        wm_changed = 1;
                    }
                }
                continue;
            }

            if (k == KEY_CTRL_F) {
                int top = wm_zcount - 1;
                if (top >= 0) {
                    Window* w = wm_get(wm_zorder[top]);
                    if (w && w->active) {
                        if (w->is_fullscreen) {
                            /* Выход из полноэкранного режима */
                            w->is_fullscreen = 0;
                            w->x = 100; w->y = 100; w->w = 600; w->h = 400;

                            /* Возвращаем окно на Рабочий стол (0) */
                            w->workspace = 0;
                            wm_current_workspace = 0;

                            /* И удаляем пустой воркспейс (если он был создан под это окно) */
                            if (wm_workspace_count > 1) wm_workspace_count--;
                        } else {
                            /* Вход в полноэкранный режим */
                            w->is_fullscreen = 1;

                            /* Создаем новый воркспейс СПРАВА */
                            w->workspace = wm_workspace_count;
                            wm_current_workspace = wm_workspace_count;
                            if (wm_workspace_count < 15) wm_workspace_count++;

                            w->x = 0; w->y = 0; w->w = 1024; w->h = 768 - 48;
                        }
                        wm_changed = 1;
                    }
                }
                continue;
            }

            if (k == KEY_ALT_TAB) {
                if (wm_zcount > 1) {
                    int top = wm_zorder[wm_zcount - 1];
                    int prev = wm_zorder[wm_zcount - 2];
                    wm_zorder[wm_zcount - 1] = prev;
                    wm_zorder[wm_zcount - 2] = top;
                    wm_changed = 1;
                }
                continue;
            }

            if (studio.open && studio.win_idx >= 0) {
                Window* sw = wm_get(studio.win_idx);
                if (sw && sw->active) { if (studio_handle_key(k)) studio_changed = 1; continue; }
            }

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
                else if (act == 2) { int idx = filer.item_indices[filer.selected]; open_nano(idx); filer_changed = 1; }
                else if (act == 3) filer_changed = 1;
                continue;
            }

            if (browser.open && (browser.url_focus || browser.doc.active_input >= 0)) {
                if (browser_handle_key(k)) browser_changed = 1;
                continue;
            }

            if (settings_app.open) {
                if (settings_handle_key(k)) { settings_changed = 1; wm_changed = 1; }
            }

            if (graphtool.open && graphtool.win_idx >= 0) {
                if (graphtool_handle_key(k)) graphtool_changed = 1;
            }

            if (nano.open) nano_desk_handle_key(k);
            else if (term.open) {
                if (k == KEY_UP)   { term_scroll(&term, +1); term_scrolled = 1; }
                else if (k == KEY_DOWN) { term_scroll(&term, -1); term_scrolled = 1; }
                else term_handle_key(&term, k);
            }
            else if (browser.open) { if (browser_handle_key(k)) browser_changed = 1; }
        }

        int right_now = right && !prev_right;

        if (right_now && !g_menu.open && filer.open && point_in_window(mouse.x, mouse.y)) {
            int mx = mouse.x, my = mouse.y;
            if (mx >= filer.cur_x && mx < filer.cur_x + filer.width && my >= filer.cur_y && my < filer.cur_y + filer.height) {
                int row = (my - filer.cur_y) / FILER_ITEM_H;
                if (row >= 0 && row < filer.item_count) { filer.selected = row; filer_start_rename(&filer); filer_changed = 1; }
            }
            prev_x = mouse.x; prev_y = mouse.y; prev_left = left; prev_right = right;
            full_redraw(&g_menu, &g_start_menu);
            continue;
        }

        if (right_now && !g_menu.open && !point_in_window(mouse.x, mouse.y)) {
            int hit = icon_hit_test(mouse.x, mouse.y);
            if (hit >= 0) menu_open(&g_menu, mouse.x, mouse.y, menu_icon_labels, menu_icon_actions, 4, hit);
            else          menu_open(&g_menu, mouse.x, mouse.y, menu_desk_labels, menu_desk_actions, 3, -1);
            full_redraw(&g_menu, &g_start_menu);
            prev_x = mouse.x; prev_y = mouse.y; prev_left = left; prev_right = right;
            continue;
        }

        if (g_menu.open) {
            int saved_target = g_menu.target_icon;
            int action = menu_update(&g_menu, mouse.x, mouse.y, left, prev_left);
            if (action != MENU_ACTION_NONE) {
                menu_action_changed = 1;
                if (action == MENU_ACTION_NEW_FILE) {
                    int d = desktop_get_folder_index(); if (d >= 0) fs_create_in(d, "new_file.txt", OBJ_FILE); desktop_scan_files();
                } else if (action == MENU_ACTION_NEW_FOLDER) {
                    int d = desktop_get_folder_index(); if (d >= 0) fs_create_in(d, "new_folder", OBJ_DIR); desktop_scan_files();
                } else if (action == MENU_ACTION_OPEN && saved_target >= 0) {
                    int fs_idx = desktop_icons[saved_target].fs_index;
                    if (fs_idx >= 0) { FsObject* o = fs_get(fs_idx); open_object(o, fs_idx); }
                } else if (action == MENU_ACTION_DELETE && saved_target >= 0) {
                    int fs_idx = desktop_icons[saved_target].fs_index; if (fs_idx >= 0) fs_delete_by_index(fs_idx); desktop_scan_files();
                } else if (action == MENU_ACTION_CREATE_LINK && saved_target >= 0) {
                    create_desktop_link(desktop_icons[saved_target].fs_index, -1, -1);
                }
            }
        }

        if (filer.open && left && !prev_left && !g_menu.open) {
            if (filer_handle_click(&filer, mouse.x, mouse.y)) filer_changed = 1;
        }

        int icon_action_changed = 0;
        if (!g_menu.open && !g_start_menu.open && !mission_is_open()) {
            int clicked_icon = desktop_icons_update(mouse.x, mouse.y, left, prev_left);
            if (clicked_icon >= 0) {
                icon_action_changed = 1;
                int fs_idx = desktop_icons[clicked_icon].fs_index;
                if (fs_idx >= 0) { FsObject* o = fs_get(fs_idx); open_object(o, fs_idx); }
            }
            for (int i = 0; i < desktop_icons_count; i++) if (desktop_icons[i].dragging) { icon_action_changed = 1; break; }
        }

        if (!g_menu.open && !g_start_menu.open && !mission_is_open()) {
            wm_update(mouse.x, mouse.y, left, prev_left);
            for (int i = 0; i < WM_MAX_WINDOWS; i++) {
                Window* w = wm_get(i);
                if (w && w->active && w->dragging) { wm_changed = 1; break; }
            }
        }

        if (term.open && term.win_idx >= 0) { Window* w = wm_get(term.win_idx); if (!w || !w->active) { term_close(&term); wm_changed = 1; } }
        if (filer.open && filer.win_idx >= 0) { Window* w = wm_get(filer.win_idx); if (!w || !w->active) { filer_close(&filer); wm_changed = 1; } }
        if (nano.open && nano.win_idx >= 0) { Window* w = wm_get(nano.win_idx); if (!w || !w->active) { nano_desk_close(); wm_changed = 1; } }
        if (graphtool.open && graphtool.win_idx >= 0) { Window* w = wm_get(graphtool.win_idx); if (!w || !w->active) { graphtool_save(); graphtool_close(); wm_changed = 1; } }
        if (browser.open && browser.win_idx >= 0) { Window* w = wm_get(browser.win_idx); if (!w || !w->active) { browser_close(); wm_changed = 1; } }
        if (settings_app.open && settings_app.win_idx >= 0) { Window* w = wm_get(settings_app.win_idx); if (!w || !w->active) { settings_close(); wm_changed = 1; } }
        if (studio.open && studio.win_idx >= 0) { Window* w = wm_get(studio.win_idx); if (!w || !w->active) { studio_close(); wm_changed = 1; } }
        if (capp_gc()) wm_changed = 1;

        int mouse_moved     = (mouse.x != prev_x || mouse.y != prev_y);
        int buttons_changed = (left != prev_left || right != prev_right);

        int need_full = (key_pressed || menu_action_changed || icon_action_changed || wm_changed || filer_changed ||
                         term_scrolled || startmenu_changed || graphtool_changed || browser_changed || settings_changed || studio_changed);

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