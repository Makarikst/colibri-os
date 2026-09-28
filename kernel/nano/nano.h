#ifndef NANO_H
#define NANO_H

#include "../vga/vga.h"
#include "../kbd/kbd.h"
#include "../fs/fs.h"
#include "../utils/utils.h"

/* ============================================
   Простой текстовый редактор
   ============================================ */

void nano_open(const char* filename);

#endif