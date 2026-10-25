#ifndef FONT8X16_H
#define FONT8X16_H
#include "../utils/utils.h"

/* ============================================
   Bitmap-шрифт 8x16 (ASCII, 256 символов)
   Здесь только ОБЪЯВЛЕНИЕ.
   Определение — в font8x16.c
   ============================================ */

/* font8x16.h */
extern const unsigned char font8x16[256][16];       /* ASCII 0x00-0x7F (rest unused) */
extern const unsigned char font_cyr[64][16];         /* U+0410..U+044F  А-Я а-я */
extern const unsigned char font_yo[2][16];           /* [0]=Ё U+0401, [1]=ё U+0451 */

const unsigned char* font_glyph(uint32_t cp);


#endif