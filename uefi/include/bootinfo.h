/* ============================================
   Colibri OS — BootInfo (передаётся ядру)
   Адрес: 0x7000
   ============================================ */

#ifndef BOOTINFO_H
#define BOOTINFO_H

typedef unsigned int   uint32_t;

typedef struct {
    uint32_t* framebuffer;    /* 0x00 */
    uint32_t  width;          /* 0x04 */
    uint32_t  height;         /* 0x08 */
    uint32_t  pitch;          /* 0x0C */
    uint32_t  bpp;            /* 0x10 */
} BootInfo;

#define BOOTINFO_ADDR 0x7000

#endif