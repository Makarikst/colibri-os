/* ============================================
   Colibri OS — управление памятью (куча)
   ============================================ */

#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

void heap_init();
void* kmalloc(size_t size);
void kfree(void* ptr);
void heap_stats();

#endif