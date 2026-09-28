#ifndef CPE_H
#define CPE_H

#include "../vga/vga.h"
#include "../kbd/kbd.h"
#include "../fs/fs.h"
#include "../utils/utils.h"

/* ============================================
   Интерпретатор .cpe (Python-подобный)
   ============================================ */

/* --- Запуск файла --- */
void cpe_run(const char* filename);

/* --- Выполнить одну строку --- */
void cpe_exec_line(const char* line);

/* --- Сброс переменных --- */
void cpe_vars_reset(void);

/* --- Запуск .cai (заглушка) --- */
void cai_run(const char* filename);

#endif