Do it in the framebuffer layer rather than in `menupkm.c`. Then the menu, GraphTool buttons and everything else that
prints text gets Cyrillic with no further changes.

## 1. Keep CP437 for ASCII and add a separate Cyrillic table

The simplest approach is not to reuse the 0x80–0xFF slots of `font8x16`. Add a second table indexed by Unicode code
point instead, which avoids code-page mapping:

```c
/* font8x16.h */
extern const unsigned char font8x16[256][16];       /* ASCII 0x00-0x7F (rest unused) */
extern const unsigned char font_cyr[64][16];         /* U+0410..U+044F  А-Я а-я */
extern const unsigned char font_yo[2][16];           /* [0]=Ё U+0401, [1]=ё U+0451 */

const unsigned char* font_glyph(uint32_t cp);
```

```c
/* font8x16.c (after the tables) */
const unsigned char* font_glyph(uint32_t cp) {
    if (cp < 0x80)                    return font8x16[cp];
    if (cp >= 0x410 && cp <= 0x44F)   return font_cyr[cp - 0x410];
    if (cp == 0x401)                  return font_yo[0];
    if (cp == 0x451)                  return font_yo[1];
    return font8x16['?'];
}
```

## 2. UTF-8 decoder

Put this in `utils.c` and `utils.h`:

```c
uint32_t utf8_next(const char** sp) {
    const unsigned char* s = (const unsigned char*)*sp;
    uint32_t cp;
    if (s[0] < 0x80) { cp = s[0]; *sp += 1; }
    else if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); *sp += 2;
    } else if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); *sp += 3;
    } else { cp = '?'; *sp += 1; }   /* invalid / 4-byte */
    return cp;
}
```

The `s[1]`/`s[2]` checks stop it reading past the NUL terminator, because 0 fails the `0x80` continuation test.

## 3. Use it in the draw functions

Change `fb_putchar_to_buffer` (and `fb_putchar`) to take a code point and call `font_glyph`:

```c
void fb_putchar_to_buffer(uint32_t* buf, int bw, int bh, uint32_t cp,
                          int x, int y, uint32_t fg, uint32_t bg) {
    const unsigned char* g = font_glyph(cp);
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 8; col++) { /* same body, bits = g[row] */ }
}

void fb_print_to_buffer(...) {
    int cx = x;
    while (*str) {
        if (*str == '\n') { cx = x; y += 16; str++; continue; }
        fb_putchar_to_buffer(buf, bw, bh, utf8_next(&str), cx, y, fg, bg);
        cx += 8;
    }
}
```

Then in `menu_draw`, replace the hand-written glyph loop with
`fb_print_to_buffer(buf, bw, bh, s, tx, ty, MENU_TEXT, MENU_BG)`. That also removes the `font8x16.h` include from
`menupkm.c`.

Callers of `fb_putchar*` that pass a `char` will still compile, since a `char` converts to `uint32_t`. Cast through
`unsigned char` if you ever pass a negative value.

## 4. Getting the glyph data

You need 66 glyphs of 16 bytes each, and I wouldn't draw them by hand. Generate them from a console font that already
has 8×16 Cyrillic, such as `Uni2-Fixed16.psf.gz` or `cyr-sun16.psf.gz` in `/usr/share/consolefonts/` (check what's
installed with `ls /usr/share/consolefonts`). A small Python script can do the conversion:

```python
# PSF2 parse: header 32 bytes: magic(4) version headersize flags length charsize height width
import gzip, struct, sys

d = gzip.open(sys.argv[1]).read()
_, _, hdr, flags, n, sz, h, w = struct.unpack('<IIIIIIII', d[:32])
assert (h, w) == (16, 8)
glyphs = [d[hdr + i * sz: hdr + (i + 1) * sz] for i in range(n)]
# Without a unicode table, glyph order follows the font's own layout
# (Uni2-Fixed16 is unicode-ordered only for the first 256 glyphs).
```

For fonts with a unicode table (`flags & 1`), parse the table after the glyphs to map `cp → glyph index`. If the font is
a legacy PSF1 `cyr-sun16`, it's laid out as CP866/KOI8, so you map CP866 order instead: А–Я = 0x80–0x9F, а–п =
0xA0–0xAF, р–я = 0xE0–0xEF, Ё/ё = 0xF0/0xF1. Print the result as `{0x..,...},` lines into `font8x16_cyr.c`. Check the
font's license before committing it. Terminus and the Linux console fonts are fine to use.

## 5. After that

- Add `font8x16_cyr.c` to your build (CMake or Makefile).
- Make sure the source files are saved as UTF-8, which they already are given the labels display correctly in the
  editor.
- Rebuild and confirm the menu labels render.
