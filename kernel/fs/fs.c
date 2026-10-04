#include "fs.h"
#include "../vga/vga.h"

static FsObject fs_objects[FS_MAX_OBJECTS];
static int current_dir = ROOT_INDEX;

FsObject* fs_get(int idx) {
    if (idx < 0 || idx >= FS_MAX_OBJECTS) return 0;
    return &fs_objects[idx];
}

int fs_get_current_dir(void) { return current_dir; }
void fs_set_current_dir(int idx) { current_dir = idx; }

int fs_find_in(int parent, const char* name) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++)
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == parent &&
            strcmp(fs_objects[i].name, name) == 0) return i;
    return -1;
}

/* ============================================
   Упаковка .capp с байт-кодом
   ============================================ */
static void pack_capp(int idx, const unsigned char* code, int code_size) {
    if (idx < 0) return;
    FsObject* o = fs_get(idx);
    if (!o) return;

    o->data[0] = 'C';
    o->data[1] = 'A';
    o->data[2] = 'P';
    o->data[3] = 'P';
    o->data[4] = 2;
    o->data[5] = 9;
    o->data[6] = 0;
    o->data[7] = (char)(code_size & 0xFF);
    o->data[8] = (char)((code_size >> 8) & 0xFF);

    for (int i = 0; i < code_size; i++)
        o->data[9 + i] = (char)code[i];

    o->size = 9 + code_size;
}

void fs_init(void) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        fs_objects[i].type = OBJ_FREE;
        fs_objects[i].name[0] = 0;
        fs_objects[i].parent = -1;
        fs_objects[i].size = 0;
        fs_objects[i].data[0] = 0;
        fs_objects[i].target_id = -1;
        fs_objects[i].saved_type = -1;
    }
    fs_objects[ROOT_INDEX].type = OBJ_DIR;
    strcpy(fs_objects[ROOT_INDEX].name, "/");
    fs_objects[ROOT_INDEX].parent = -1;

    int c = 1; strcpy(fs_objects[c].name, "colibri"); fs_objects[c].type = OBJ_DIR; fs_objects[c].parent = ROOT_INDEX;
    int u = 2; strcpy(fs_objects[u].name, "users");   fs_objects[u].type = OBJ_DIR; fs_objects[u].parent = c;
    int a = 3; strcpy(fs_objects[a].name, "alpha");   fs_objects[a].type = OBJ_DIR; fs_objects[a].parent = u;
    int s = 4; strcpy(fs_objects[s].name, "system");  fs_objects[s].type = OBJ_DIR; fs_objects[s].parent = c;

    current_dir = a;

    int desktop_idx = fs_find_in(a, "Desktop");
    if (desktop_idx < 0) desktop_idx = fs_create_in(a, "Desktop", OBJ_DIR);

    int apps_idx = fs_find_in(a, "Applications");
    if (apps_idx < 0) apps_idx = fs_create_in(a, "Applications", OBJ_DIR);

    if (fs_find_in(a, "Trash") < 0)
        fs_create_in(a, "Trash", OBJ_DIR);

    int term_app = -1, nano_app = -1, filer_app = -1;
    if (apps_idx >= 0) {
        term_app  = fs_find_in(apps_idx, "Terminal.capp");
        if (term_app < 0) term_app = fs_create_in(apps_idx, "Terminal.capp", OBJ_APP_TERMINAL);

        nano_app  = fs_find_in(apps_idx, "Nano.capp");
        if (nano_app < 0) nano_app = fs_create_in(apps_idx, "Nano.capp", OBJ_APP_NANO);

        filer_app = fs_find_in(apps_idx, "Filer.capp");
        if (filer_app < 0) filer_app = fs_create_in(apps_idx, "Filer.capp", OBJ_APP_FILER);
    }
    int graph_app = -1;
    if (apps_idx >= 0) {
        graph_app = fs_find_in(apps_idx, "GraphTool.capp");
        if (graph_app < 0)
            graph_app = fs_create_in(apps_idx, "GraphTool.capp", OBJ_APP_GRAPHTOOL);
    }

    if (desktop_idx >= 0) {
        if (fs_find_in(desktop_idx, "Terminal.yrl") < 0) {
            int l = fs_create_in(desktop_idx, "Terminal.yrl", OBJ_LINK);
            if (l >= 0) fs_objects[l].target_id = term_app;
        }
        if (fs_find_in(desktop_idx, "Nano.yrl") < 0) {
            int l = fs_create_in(desktop_idx, "Nano.yrl", OBJ_LINK);
            if (l >= 0) fs_objects[l].target_id = nano_app;
        }
        if (fs_find_in(desktop_idx, "Filer.yrl") < 0) {
            int l = fs_create_in(desktop_idx, "Filer.yrl", OBJ_LINK);
            if (l >= 0) fs_objects[l].target_id = filer_app;
        }
        if (fs_find_in(desktop_idx, "GraphTool.yrl") < 0) {
            int l = fs_create_in(desktop_idx, "GraphTool.yrl", OBJ_LINK);
            if (l >= 0) fs_objects[l].target_id = graph_app;
        }
        if (fs_find_in(desktop_idx, "Trash.yrl") < 0) {
            int trash_idx = fs_find_in(a, "Trash");
            int l = fs_create_in(desktop_idx, "Trash.yrl", OBJ_LINK);
            if (l >= 0) fs_objects[l].target_id = trash_idx;
        }
    }

    /* Байт-код для приложений */
    static const unsigned char code_terminal[] = { 0x10, 0xFF };
    static const unsigned char code_nano[]     = { 0x11, 0xFF };
    static const unsigned char code_filer[]    = { 0x12, 0xFF };
    static const unsigned char code_graph[] = { 0x13, 0xFF };

    pack_capp(term_app,  code_terminal, sizeof(code_terminal));
    pack_capp(nano_app,  code_nano,     sizeof(code_nano));
    pack_capp(filer_app, code_filer,    sizeof(code_filer));
    pack_capp(graph_app, code_graph, sizeof(code_graph));

    /* Содержимое .yrl — ID цели */
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type == OBJ_LINK) {
            char buf[16];
            itoa(fs_objects[i].target_id, buf, 10);
            int n = 0; while (buf[n] && n < FS_DATA_LEN - 1) { fs_objects[i].data[n] = buf[n]; n++; }
            fs_objects[i].data[n] = 0;
            fs_objects[i].size = n;
        }
    }
}

int fs_create_in(int parent_idx, const char* name, int type) {
    if (fs_find_in(parent_idx, name) != -1) return -1;
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type == OBJ_FREE) {
            strncpy(fs_objects[i].name, name, FS_NAME_LEN - 1);
            fs_objects[i].type = type;
            fs_objects[i].parent = parent_idx;
            fs_objects[i].size = 0;
            fs_objects[i].data[0] = 0;
            fs_objects[i].target_id = -1;
            fs_objects[i].saved_type = -1;

            /* Если это приложение — запоминаем исходный тип */
            if (type == OBJ_APP_TERMINAL || type == OBJ_APP_NANO ||
                type == OBJ_APP_FILER    || type == OBJ_APP_TRASH) {
                fs_objects[i].saved_type = type;
            }
            return i;
        }
    }
    return -1;
}

void fs_delete_by_index(int idx) {
    if (idx == ROOT_INDEX) return;
    if (fs_objects[idx].type == OBJ_DIR)
        for (int i = 0; i < FS_MAX_OBJECTS; i++)
            if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == idx)
                fs_delete_by_index(i);
    fs_objects[idx].type = OBJ_FREE;
    fs_objects[idx].name[0] = 0;
    fs_objects[idx].parent = -1;
    fs_objects[idx].target_id = -1;
    fs_objects[idx].saved_type = -1;
}

void strip_quotes(const char* in, char* out) {
    int i = 0, j = 0;
    while (in[i] == ' ' || in[i] == '\t') i++;
    if (in[i] == '"') {
        i++;
        while (in[i] && in[i] != '"' && j < 255) out[j++] = in[i++];
    } else {
        while (in[i] && in[i] != ' ' && in[i] != '\t' && j < 255) out[j++] = in[i++];
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
                else { int idx = fs_find_in(cur, part); if (idx == -1) return -1; cur = idx; }
            }
            if (*p == 0) break;
            pi = 0; p++;
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
                    else { int idx = fs_find_in(last_parent, last_name); if (idx == -1) return 0; last_parent = idx; }
                }
                strcpy(last_name, part);
            }
            if (*p == 0) break;
            pi = 0; p++;
            continue;
        }
        if (pi < FS_NAME_LEN - 1) part[pi++] = *p;
        p++;
    }
    *parent_idx = last_parent;
    strncpy(last, last_name, FS_NAME_LEN - 1);
    return (last_name[0] != 0);
}

void tree_recursive(int dir_idx, int depth) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == dir_idx) {
            for (int j = 0; j < depth; j++) vga_print("  ");
            if (fs_objects[i].type == OBJ_DIR) {
                vga_print_color("[D] ", VGA_LIGHT_CYAN);
                vga_print_color(fs_objects[i].name, VGA_LIGHT_CYAN);
                vga_putchar('\n');
                tree_recursive(i, depth + 1);
            } else {
                vga_print("[F] ");
                vga_print(fs_objects[i].name);
                vga_putchar('\n');
            }
        }
    }
}

void find_recursive(int dir_idx, const char* name, int* found) {
    for (int i = 0; i < FS_MAX_OBJECTS; i++) {
        if (fs_objects[i].type != OBJ_FREE && fs_objects[i].parent == dir_idx) {
            if (strcmp(fs_objects[i].name, name) == 0) {
                vga_print("  ");
                if (fs_objects[i].type == OBJ_DIR) vga_print_color("[D] ", VGA_LIGHT_CYAN);
                else vga_print("[F] ");
                vga_print(fs_objects[i].name);
                vga_putchar('\n');
                *found = 1;
            }
            if (fs_objects[i].type == OBJ_DIR) find_recursive(i, name, found);
        }
    }
}