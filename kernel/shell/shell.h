#ifndef SHELL_H
#define SHELL_H

#include "../vga/vga.h"
#include "../kbd/kbd.h"
#include "../fs/fs.h"
#include "../commands/commands.h"
#include "../utils/utils.h"

/* ============================================
   Оболочка Colibri
   ============================================ */

#define LINE_MAX 256
#define HIST_MAX 16

/* --- Запуск --- */
void shell_run(void);

/* --- Внутренние --- */
void shell_prompt(void);
void shell_execute(char* line);
void shell_history_show(void);
void shell_history_add(const char* line);

#endif