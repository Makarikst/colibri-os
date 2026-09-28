#include "cpe.h"

/* ============================================
   Переменные интерпретатора
   ============================================ */
#define CPE_MAX_VARS 32
#define CPE_VAR_NAME 32
#define CPE_VAR_VAL  256

typedef struct {
    char name[CPE_VAR_NAME];
    char value[CPE_VAR_VAL];
    int  is_string;
    int  is_number;
    int  number;
} CpeVar;

static CpeVar cpe_vars[CPE_MAX_VARS];
static int    cpe_var_count = 0;

/* ============================================
   Сброс переменных
   ============================================ */
void cpe_vars_reset(void) {
    cpe_var_count = 0;
    for (int i = 0; i < CPE_MAX_VARS; i++) {
        cpe_vars[i].name[0] = 0;
        cpe_vars[i].value[0] = 0;
        cpe_vars[i].is_string = 0;
        cpe_vars[i].is_number = 0;
        cpe_vars[i].number = 0;
    }
}

/* ============================================
   Поиск переменной
   ============================================ */
static CpeVar* cpe_find_var(const char* name) {
    for (int i = 0; i < cpe_var_count; i++)
        if (strcmp(cpe_vars[i].name, name) == 0) return &cpe_vars[i];
    return 0;
}

/* ============================================
   Установка переменной
   ============================================ */
static void cpe_set_var(const char* name, const char* value,
                        int is_string, int is_number, int number) {
    CpeVar* v = cpe_find_var(name);
    if (!v) {
        if (cpe_var_count >= CPE_MAX_VARS) return;
        v = &cpe_vars[cpe_var_count++];
        strcpy(v->name, name);
    }
    strcpy(v->value, value);
    v->is_string = is_string;
    v->is_number = is_number;
    v->number    = number;
}

/* ============================================
   Лексер: пропуск пробелов
   ============================================ */
static const char* cpe_skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* ============================================
   Чтение строки в кавычках
   ============================================ */
static const char* cpe_read_string(const char* p, char* out) {
    p = cpe_skip_ws(p);
    if (*p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < CPE_VAR_VAL - 1) out[i++] = *p++;
    out[i] = 0;
    if (*p == '"') p++;
    return p;
}

/* ============================================
   Чтение токена (идентификатор / число)
   ============================================ */
static const char* cpe_read_token(const char* p, char* out) {
    p = cpe_skip_ws(p);
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
           *p != '(' && *p != ')' && *p != '=' && *p != '"' &&
           *p != '+' && *p != '-' && *p != '*' && *p != '/' &&
           *p != ',' && i < CPE_VAR_VAL - 1) {
        out[i++] = *p++;
    }
    out[i] = 0;
    return p;
}

/* ============================================
   Числа
   ============================================ */
static int cpe_parse_int(const char* s, int* out) {
    int sign = 1;
    int i = 0;
    if (s[0] == '-') { sign = -1; i = 1; }
    if (s[i] == 0) return 0;
    int n = 0;
    while (s[i]) {
        if (s[i] < '0' || s[i] > '9') return 0;
        n = n * 10 + (s[i] - '0');
        i++;
    }
    *out = n * sign;
    return 1;
}

static int cpe_get_number(const char* token, int* out) {
    int n;
    if (cpe_parse_int(token, &n)) { *out = n; return 1; }
    CpeVar* v = cpe_find_var(token);
    if (v && v->is_number) { *out = v->number; return 1; }
    return 0;
}

/* ============================================
   Арифметика
   ============================================ */
static int cpe_eval_expr(const char* expr, int* out) {
    const char* p = expr;
    int have_left = 0;
    int result = 0;
    int op = 0;

    while (*p) {
        p = cpe_skip_ws(p);
        if (*p == 0) break;

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
            op = (*p == '+') ? 1 : (*p == '-') ? 2 : (*p == '*') ? 3 : 4;
            p++;
            continue;
        }

        char token[CPE_VAR_VAL];
        p = cpe_read_token(p, token);
        if (token[0] == 0) break;

        int val;
        if (!cpe_get_number(token, &val)) return 0;

        if (!have_left) { result = val; have_left = 1; }
        else {
            if      (op == 1) result += val;
            else if (op == 2) result -= val;
            else if (op == 3) result *= val;
            else if (op == 4) { if (val != 0) result /= val; }
            op = 0;
        }
    }
    if (!have_left) return 0;
    *out = result;
    return 1;
}

/* ============================================
   Ввод строки
   ============================================ */
static char cpe_input_buffer[CPE_VAR_VAL];

static const char* cpe_input(const char* prompt) {
    vga_print(prompt);
    int i = 0;
    while (1) {
        char c = kbd_get_key();
        if (c == '\n') {
            cpe_input_buffer[i] = 0;
            vga_putchar('\n');
            return cpe_input_buffer;
        }
        if (c == '\b') {
            if (i > 0) { i--; vga_putchar('\b'); }
            continue;
        }
        if ((unsigned char)c < 32) continue;
        if (i < CPE_VAR_VAL - 1) {
            cpe_input_buffer[i++] = c;
            vga_putchar(c);
        }
    }
}

/* ============================================
   Выполнить строку
   ============================================ */
void cpe_exec_line(const char* line) {
    const char* p = cpe_skip_ws(line);
    if (*p == 0 || *p == '#') return;

    /* --- print(...) --- */
    if (strncmp(p, "print", 5) == 0) {
        p = cpe_skip_ws(p + 5);
        if (*p == '(') {
            p++;
            p = cpe_skip_ws(p);

            /* print("строка") */
            if (*p == '"') {
                char buf[CPE_VAR_VAL];
                p = cpe_read_string(p, buf);
                vga_print(buf);
                vga_putchar('\n');
                return;
            }

            /* print(math.sqrt(...)) и т.д. */
            if (strncmp(p, "math.", 5) == 0) {
                p += 5;
                char fn[16];
                int i = 0;
                while (*p && *p != '(' && i < 15) fn[i++] = *p++;
                fn[i] = 0;
                p = cpe_skip_ws(p);
                if (*p == '(') p++;

                char arg1[CPE_VAR_VAL], arg2[CPE_VAR_VAL];
                int a1 = 0, a2 = 0;

                p = cpe_read_token(p, arg1);
                cpe_get_number(arg1, &a1);

                p = cpe_skip_ws(p);
                if (*p == ',') {
                    p++;
                    p = cpe_read_token(p, arg2);
                    cpe_get_number(arg2, &a2);
                }

                if      (strcmp(fn, "sqrt") == 0) { int r = 0; for (int k = 1; k * k <= a1; k++) r = k; vga_print_dec(r); }
                else if (strcmp(fn, "abs")  == 0) { vga_print_dec(a1 < 0 ? -a1 : a1); }
                else if (strcmp(fn, "min")  == 0) { vga_print_dec(a1 < a2 ? a1 : a2); }
                else if (strcmp(fn, "max")  == 0) { vga_print_dec(a1 > a2 ? a1 : a2); }
                vga_putchar('\n');
                return;
            }

            /* print(переменная) */
            char token[CPE_VAR_VAL];
            p = cpe_read_token(p, token);
            if (token[0]) {
                CpeVar* v = cpe_find_var(token);
                if (v) {
                    if (v->is_number) vga_print_dec(v->number);
                    else vga_print(v->value);
                    vga_putchar('\n');
                } else {
                    vga_print(token);
                    vga_putchar('\n');
                }
            }
            return;
        }
    }

    /* --- input(...) --- */
    if (strncmp(p, "input", 5) == 0) {
        p = cpe_skip_ws(p + 5);
        if (*p == '(') {
            p++;
            p = cpe_skip_ws(p);
            char prompt[CPE_VAR_VAL];
            if (*p == '"') {
                p = cpe_read_string(p, prompt);
                const char* result = cpe_input(prompt);
                cpe_set_var("_", result, 1, 0, 0);
            }
            return;
        }
    }

    /* --- Присваивание --- */
    char name[CPE_VAR_NAME];
    const char* q = cpe_read_token(p, name);
    q = cpe_skip_ws(q);
    if (*q == '=') {
        q++;
        q = cpe_skip_ws(q);

        if (*q == '"') {
            char buf[CPE_VAR_VAL];
            q = cpe_read_string(q, buf);
            cpe_set_var(name, buf, 1, 0, 0);
        }
        else if (strncmp(q, "input", 5) == 0) {
            q = cpe_skip_ws(q + 5);
            if (*q == '(') {
                q++;
                q = cpe_skip_ws(q);
                char prompt[CPE_VAR_VAL];
                if (*q == '"') {
                    q = cpe_read_string(q, prompt);
                    const char* result = cpe_input(prompt);
                    cpe_set_var(name, result, 1, 0, 0);
                }
            }
        }
        else {
            int num;
            if (cpe_eval_expr(q, &num)) {
                char buf[16];
                itoa(num, buf, 10);
                cpe_set_var(name, buf, 0, 1, num);
            } else {
                char buf[CPE_VAR_VAL];
                q = cpe_read_token(q, buf);
                cpe_set_var(name, buf, 0, 0, 0);
            }
        }
    }
}

/* ============================================
   Запуск файла .cpe
   ============================================ */
void cpe_run(const char* filename) {
    int parent; char last[FS_NAME_LEN];
    if (!split_path(filename, fs_get_current_dir(), &parent, last)) {
        vga_print("cpe: invalid name\n");
        return;
    }
    int idx = fs_find_in(parent, last);
    if (idx == -1 || fs_get(idx)->type != OBJ_FILE) {
        vga_print_color("cpe: file not found: ", VGA_LIGHT_RED);
        vga_print(filename);
        vga_putchar('\n');
        return;
    }

    FsObject* f = fs_get(idx);

    cpe_vars_reset();

    char line[256];
    int line_len = 0;

    for (int i = 0; i < f->size; i++) {
        char c = f->data[i];
        if (c == '\n' || line_len >= 255) {
            line[line_len] = 0;
            cpe_exec_line(line);
            line_len = 0;
        } else {
            line[line_len++] = c;
        }
    }
    if (line_len > 0) {
        line[line_len] = 0;
        cpe_exec_line(line);
    }
}

/* ============================================
   .cai — заглушка
   ============================================ */
void cai_run(const char* filename) {
    vga_print_color("cai: ", VGA_YELLOW);
    vga_print(filename);
    vga_putchar('\n');
    vga_print("CAI runtime will be implemented in v1.0.\n");
}