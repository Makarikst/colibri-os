/* ============================================
   Colibri OS — управление памятью (куча)
   ============================================ */

#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

/* Инициализация кучи */
void heap_init();

/* Выделить память */
void* kmalloc(size_t size);

/* Освободить память */
void kfree(void* ptr);

/* Статистика кучи */
void heap_stats();

#endif