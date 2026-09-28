#ifndef KMALLOC_H
#define KMALLOC_H

#include "../utils/utils.h"

/* ============================================
   Простой bump-allocator с free-list
   ============================================ */

/* Инициализация кучи */
void heap_init(void);

/* Выделить память */
void* kmalloc(unsigned int size);

/* Освободить память */
void kfree(void* ptr);

/* Статистика кучи */
void heap_stats(void);

#endif