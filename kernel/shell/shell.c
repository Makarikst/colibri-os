#include "shell.h"
#include "../nano/nano.h"
#include "../cpe/cpe.h"

/* ============================================
   История команд
   ============================================ */
static char history[HIST_MAX][LINE_MAX];
static int history_count = 0;

void shell_history_add(const char* line) {
    if (!line || !line[0]) return;
    if (history_count > 0 && strcmp(history[history_count-1], line) == 0) return;
    if (history_count < HIST_MAX) {
        strncpy(history[history_count], line, LINE_MAX - 1);
        history[history_count][LINE_MAX-1] = 0;
        history_count++;
    } else {
        for (int i = 0; i < HIST_MAX - 1; i++) strcpy(history[i], history[i+1]);
        strncpy(history[HIST_MAX-1], line, LINE_MAX - 1);
        history[HIST_MAX-1][LINE_MAX-1] = 0;
    }
}

void shell_history_show(void) {
    for (int i = 0; i < history_count; i++) {
        vga_print_dec(i + 1);
        vga_print("  ");
        vga_print(history[i]);
        vga_putchar('\n');
    }
}

/* ============================================
   Приглашение
   ============================================ */
void shell_prompt(void) {
    vga_print_color("colibri:", VGA_LIGHT_GREEN);
    cmd_pwd_no_newline();
    vga_print("> ");
}

/* ============================================
   Парсер строки
   ============================================ */
static int tokenize(char* line, char tokens[][128], int max_tokens) {
    int n = 0;
    char* p = line;
    while (*p && n < max_tokens) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < 127) tokens[n][i++] = *p++;
        tokens[n][i] = 0;
        n++;
    }
    return n;
}

static int find_redirect(char tokens[][128], int n, int* append) {
    for (int i = 0; i < n; i++) {
        if (strcmp(tokens[i], ">") == 0) { *append = 0; return i; }
        if (strcmp(tokens[i], ">>") == 0) { *append = 1; return i; }
    }
    return -1;
}

/* ============================================
   Выполнение команды
   ============================================ */
static char expr_buf[LINE_MAX];

void shell_execute(char* line) {
    trim(line);
    if (!line[0]) return;

    /* Alias */
    const char* al = commands_alias_get(line);
    if (al) {
        char tmp[LINE_MAX];
        strncpy(tmp, al, LINE_MAX - 1);
        tmp[LINE_MAX-1] = 0;
        shell_execute(tmp);
        return;
    }

    char cmd_buf[128];
    const char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    int ci = 0;
    while (*p && *p != ' ' && *p != '\t' && ci < 127) cmd_buf[ci++] = *p++;
    cmd_buf[ci] = 0;

    char tokens[16][128];
    int n = tokenize(line, tokens, 16);
    if (n == 0) return;

    const char* cmd = cmd_buf;

    /* Редирект */
    int append = 0;
    int redir = find_redirect(tokens, n, &append);
    if (redir >= 0) {
        char text[LINE_MAX] = {0};
        for (int i = 1; i < redir; i++) {
            if (i > 1) strncat(text, " ", LINE_MAX - strlen(text) - 1);
            strncat(text, tokens[i], LINE_MAX - strlen(text) - 1);
        }
        if (redir + 1 < n) cmd_echo_redirect(text, tokens[redir + 1], append);
        else vga_print("Missing filename\n");
        return;
    }

    /* ============================================
       System
       ============================================ */
    if (strcmp(cmd, "help") == 0)            cmd_help();
    else if (strcmp(cmd, "ver") == 0)        cmd_ver();
    else if (strcmp(cmd, "banner") == 0)     cmd_banner();
    else if (strcmp(cmd, "clear") == 0)      cmd_clear();
    else if (strcmp(cmd, "date") == 0)       cmd_date();
    else if (strcmp(cmd, "uptime") == 0)     cmd_uptime();
    else if (strcmp(cmd, "mem") == 0)        cmd_mem();
    else if (strcmp(cmd, "heap") == 0)       cmd_heap();
    else if (strcmp(cmd, "history") == 0)    shell_history_show();
    else if (strcmp(cmd, "color") == 0)      { if (n > 1) cmd_color(tokens[1]); else vga_print("Usage: color <0-15>\n"); }
    else if (strcmp(cmd, "reboot") == 0)     cmd_reboot();
    else if (strcmp(cmd, "shutdown") == 0)   cmd_shutdown();

    /* ============================================
       Executable
       ============================================ */
    else if (strcmp(cmd, "nano") == 0) { if (n > 1) nano_open(tokens[1]); else vga_print("Usage: nano <file>\n"); }
    else if (strcmp(cmd, "cpe") == 0)  { if (n > 1) cpe_run(tokens[1]);  else vga_print("Usage: cpe <file.cpe>\n"); }
    else if (strcmp(cmd, "run") == 0)  { if (n > 1) cai_run(tokens[1]);  else vga_print("Usage: run <file.cai>\n"); }

    /* ============================================
       Files
       ============================================ */
    else if (strcmp(cmd, "ls") == 0)     cmd_ls(n > 1 ? tokens[1] : "");
    else if (strcmp(cmd, "pwd") == 0)    cmd_pwd();
    else if (strcmp(cmd, "cd") == 0)     { if (n > 1) cmd_cd(tokens[1]); else cmd_cd("/"); }
    else if (strcmp(cmd, "mkdir") == 0)  { if (n > 1) cmd_mkdir(tokens[1]); else vga_print("Usage: mkdir X\n"); }
    else if (strcmp(cmd, "rmdir") == 0)  { if (n > 1) cmd_rmdir(tokens[1]); else vga_print("Usage: rmdir X\n"); }
    else if (strcmp(cmd, "touch") == 0)  { if (n > 1) cmd_touch(tokens[1]); else vga_print("Usage: touch X\n"); }
    else if (strcmp(cmd, "rm") == 0)     { if (n > 1) cmd_rm(tokens[1]); else vga_print("Usage: rm X\n"); }
    else if (strcmp(cmd, "cat") == 0)    { if (n > 1) cmd_cat(tokens[1]); else vga_print("Usage: cat <file>\n"); }
    else if (strcmp(cmd, "write") == 0) {
        if (n < 3) { vga_print("Usage: write <file> <text>\n"); }
        else {
            char text[LINE_MAX] = {0};
            for (int i = 2; i < n; i++) {
                if (i > 2) strncat(text, " ", LINE_MAX - strlen(text) - 1);
                strncat(text, tokens[i], LINE_MAX - strlen(text) - 1);
            }
            cmd_write(tokens[1], text);
        }
    }
    else if (strcmp(cmd, "append") == 0) {
        if (n < 3) { vga_print("Usage: append <file> <text>\n"); }
        else {
            char text[LINE_MAX] = {0};
            for (int i = 2; i < n; i++) {
                if (i > 2) strncat(text, " ", LINE_MAX - strlen(text) - 1);
                strncat(text, tokens[i], LINE_MAX - strlen(text) - 1);
            }
            cmd_append(tokens[1], text);
        }
    }
    else if (strcmp(cmd, "trunc") == 0)  { if (n > 1) cmd_trunc(tokens[1]); else vga_print("Usage: trunc <file>\n"); }
    else if (strcmp(cmd, "cp") == 0)     { if (n > 2) cmd_cp(tokens[1], tokens[2]); else vga_print("Usage: cp <src> <dst>\n"); }
    else if (strcmp(cmd, "mv") == 0)     { if (n > 2) cmd_mv(tokens[1], tokens[2]); else vga_print("Usage: mv <src> <dst>\n"); }
    else if (strcmp(cmd, "tree") == 0)   cmd_tree();
    else if (strcmp(cmd, "find") == 0)   { if (n > 1) cmd_find(tokens[1]); else vga_print("Usage: find <name>\n"); }
    else if (strcmp(cmd, "stat") == 0)   { if (n > 1) cmd_stat(tokens[1]); else vga_print("Usage: stat <file>\n"); }
    else if (strcmp(cmd, "wc") == 0)     { if (n > 1) cmd_wc(tokens[1]); else vga_print("Usage: wc <file>\n"); }
    else if (strcmp(cmd, "head") == 0)   { if (n > 1) cmd_head(tokens[1]); else vga_print("Usage: head <file>\n"); }
    else if (strcmp(cmd, "tail") == 0)   { if (n > 1) cmd_tail(tokens[1]); else vga_print("Usage: tail <file>\n"); }
    else if (strcmp(cmd, "grep") == 0)   { if (n > 2) cmd_grep(tokens[1], tokens[2]); else vga_print("Usage: grep <pat> <file>\n"); }
    else if (strcmp(cmd, "sort") == 0)   { if (n > 1) cmd_sort(tokens[1]); else vga_print("Usage: sort <file>\n"); }
    else if (strcmp(cmd, "hexdump") == 0){ if (n > 1) cmd_hexdump(tokens[1]); else vga_print("Usage: hexdump <file>\n"); }

    /* ============================================
       Math
       ============================================ */
    else if (strcmp(cmd, "calc") == 0) {
        if (n < 2) { vga_print("Usage: calc <expr>\n"); }
        else {
            expr_buf[0] = 0;
            for (int i = 1; i < n; i++) {
                if (i > 1) strncat(expr_buf, " ", LINE_MAX - strlen(expr_buf) - 1);
                strncat(expr_buf, tokens[i], LINE_MAX - strlen(expr_buf) - 1);
            }
            cmd_calc(expr_buf);
        }
    }
    else if (strcmp(cmd, "sqrt") == 0)   { if (n > 1) cmd_sqrt(tokens[1]); else vga_print("Usage: sqrt <n>\n"); }
    else if (strcmp(cmd, "pow") == 0)    { if (n > 2) cmd_pow(tokens[1], tokens[2]); else vga_print("Usage: pow <a> <b>\n"); }
    else if (strcmp(cmd, "rand") == 0)   cmd_rand();
    else if (strcmp(cmd, "prime") == 0)  { if (n > 1) cmd_prime(tokens[1]); else vga_print("Usage: prime <n>\n"); }
    else if (strcmp(cmd, "gcd") == 0)    { if (n > 2) cmd_gcd(tokens[1], tokens[2]); else vga_print("Usage: gcd <a> <b>\n"); }
    else if (strcmp(cmd, "lcm") == 0)    { if (n > 2) cmd_lcm(tokens[1], tokens[2]); else vga_print("Usage: lcm <a> <b>\n"); }
    else if (strcmp(cmd, "hex") == 0)    { if (n > 1) cmd_hex(tokens[1]); else vga_print("Usage: hex <n>\n"); }
    else if (strcmp(cmd, "bin") == 0)    { if (n > 1) cmd_bin(tokens[1]); else vga_print("Usage: bin <n>\n"); }

    /* ============================================
       Misc
       ============================================ */
    else if (strcmp(cmd, "echo") == 0)   cmd_echo_args(n, tokens);
    else if (strcmp(cmd, "sleep") == 0)  { if (n > 1) cmd_sleep(tokens[1]); else vga_print("Usage: sleep <ms>\n"); }
    else if (strcmp(cmd, "beep") == 0)   cmd_beep();

    /* ============================================
       Process
       ============================================ */
    else if (strcmp(cmd, "ps") == 0)     cmd_ps();
    else if (strcmp(cmd, "spawn") == 0)  { if (n > 1) cmd_spawn(tokens[1]); else vga_print("Usage: spawn <name>\n"); }
    else if (strcmp(cmd, "kill") == 0)   { if (n > 1) cmd_kill(tokens[1]); else vga_print("Usage: kill <pid>\n"); }

    /* ============================================
       Net
       ============================================ */
    else if (strcmp(cmd, "ifconfig") == 0) cmd_ifconfig();
    else if (strcmp(cmd, "ping") == 0)   { if (n > 1) cmd_ping(tokens[1]); else vga_print("Usage: ping <host>\n"); }

    /* ============================================
       Env / Alias
       ============================================ */
    else if (strcmp(cmd, "env") == 0)        cmd_env();
    else if (strcmp(cmd, "setenv") == 0)     { if (n > 1) cmd_setenv(tokens[1]); else vga_print("Usage: setenv K=V\n"); }
    else if (strcmp(cmd, "unsetenv") == 0)   { if (n > 1) cmd_unsetenv(tokens[1]); else vga_print("Usage: unsetenv K\n"); }
    else if (strcmp(cmd, "alias") == 0) {
        if (n == 1) cmd_alias_list();
        else cmd_alias_set(tokens[1]);
    }
    else if (strcmp(cmd, "unalias") == 0)    { if (n > 1) cmd_unalias(tokens[1]); else vga_print("Usage: unalias X\n"); }

    /* ============================================
       Unknown
       ============================================ */
    else {
        vga_print("Unknown command: ");
        vga_print(cmd);
        vga_putchar('\n');
        vga_print("Type 'help' for list.\n");
    }
}

/* ============================================
   Главный цикл
   ============================================ */
static char input_buf[LINE_MAX];
static int input_len = 0;

void shell_run(void) {
    while (1) {
        shell_prompt();
        input_len = 0;
        input_buf[0] = 0;

        while (1) {
            char c = kbd_get_key();

            if (c == KEY_UP)   { vga_scroll_up();   continue; }
            if (c == KEY_DOWN) { vga_scroll_down(); continue; }

            if (c == '\n') {
                vga_putchar('\n');
                input_buf[input_len] = 0;
                break;
            }
            if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    input_buf[input_len] = 0;
                    vga_putchar('\b');
                }
                continue;
            }
            if (c == KEY_LEFT || c == KEY_RIGHT || c == KEY_TAB) continue;
            if ((unsigned char)c < 32) continue;
            if (input_len < LINE_MAX - 1) {
                input_buf[input_len++] = c;
                vga_putchar(c);
            }
        }

        shell_history_add(input_buf);
        shell_execute(input_buf);
    }
}