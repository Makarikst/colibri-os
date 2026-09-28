#ifndef COMMANDS_H
#define COMMANDS_H

#include "../fs/fs.h"
#include "../vga/vga.h"
#include "../kbd/kbd.h"
#include "../kmalloc/kmalloc.h"
#include "../utils/utils.h"

/* ============================================
   Все команды оболочки
   ============================================ */

/* --- System --- */
void cmd_help(void);
void cmd_ver(void);
void cmd_banner(void);
void cmd_clear(void);
void cmd_date(void);
void cmd_uptime(void);
void cmd_mem(void);
void cmd_heap(void);
void cmd_history(void);
void cmd_color(const char* arg);
void cmd_reboot(void);
void cmd_shutdown(void);

/* --- Executable --- */
void cmd_nano(const char* filename);
void cmd_cpe(const char* filename);
void cmd_run(const char* filename);

/* --- Files --- */
void cmd_ls(const char* path);
void cmd_pwd(void);
void cmd_cd(const char* path);
void cmd_mkdir(const char* name);
void cmd_rmdir(const char* name);
void cmd_touch(const char* name);
void cmd_rm(const char* name);
void cmd_cat(const char* path);
void cmd_write(const char* name, const char* text);
void cmd_append(const char* name, const char* text);
void cmd_trunc(const char* name);
void cmd_cp(const char* src, const char* dst);
void cmd_mv(const char* src, const char* dst);
void cmd_tree(void);
void cmd_find(const char* name);
void cmd_stat(const char* name);
void cmd_wc(const char* name);
void cmd_head(const char* name);
void cmd_tail(const char* name);
/* --- Публичные обёртки для shell.c --- */
const char* commands_alias_get(const char* name);
void commands_env_show(void);
void commands_alias_show(void);
void cmd_grep(const char* pat, const char* name);
void cmd_sort(const char* name);
void cmd_hexdump(const char* name);
void cmd_echo_redirect(const char* text, const char* filename, int append);

/* --- Math --- */
void cmd_calc(const char* expr);
void cmd_sqrt(const char* arg);
void cmd_pow(const char* a, const char* b);
void cmd_rand(void);
void cmd_prime(const char* arg);
void cmd_gcd(const char* a, const char* b);
void cmd_lcm(const char* a, const char* b);
void cmd_hex(const char* arg);
void cmd_bin(const char* arg);

/* --- Misc --- */
void cmd_echo_args(int n, char tokens[][128]);
void cmd_sleep(const char* arg);
void cmd_beep(void);

/* --- Process (stub) --- */
void cmd_ps(void);
void cmd_spawn(const char* name);
void cmd_kill(const char* arg);

/* --- Net (stub) --- */
void cmd_ifconfig(void);
void cmd_ping(const char* host);

/* --- Env / Alias --- */
void cmd_env(void);
void cmd_setenv(const char* kv);
void cmd_unsetenv(const char* key);
void cmd_alias_list(void);
void cmd_alias_set(const char* kv);
void cmd_unalias(const char* name);

/* --- Вспомогательные --- */
void cmd_pwd_no_newline(void);

#endif