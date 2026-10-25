#!/usr/bin/env python3
"""Генерирует kernel/framebuffer/font8x16_cyr.c из PSF1/PSF2 консольного шрифта 8x16.
Использование: gen_cyr_font.py /usr/share/consolefonts/Uni2-Terminus16.psf.gz > font8x16_cyr.c
"""
import gzip, struct, sys

d = gzip.open(sys.argv[1]).read()
if d[:2] == b'\x36\x04':                       # PSF1
    mode, sz = d[2], d[3]
    n, hdr, w, h = (512 if mode & 1 else 256), 4, 8, d[3]
    has_uni = bool(mode & 6)
    u16 = True
elif d[:4] == b'\x72\xb5\x4a\x86':             # PSF2
    _, _, hdr, flags, n, sz, h, w = struct.unpack('<8I', d[:32])
    has_uni = bool(flags & 1)
    u16 = False
else:
    sys.exit('не PSF')
assert (w, h) == (8, 16) and has_uni, 'нужен шрифт 8x16 с unicode-таблицей'

glyph = lambda i: d[hdr + i * sz: hdr + (i + 1) * sz]
cmap, pos = {}, hdr + n * sz
for i in range(n):
    while True:
        if u16:
            v = struct.unpack_from('<H', d, pos)[0]; pos += 2
            if v == 0xFFFF: break
            if v == 0xFFFE: continue           # последовательности пропускаем
            cmap.setdefault(v, i)
        else:
            b = d[pos]
            if b == 0xFF: pos += 1; break
            if b == 0xFE: pos += 1; continue
            l = 1 if b < 0x80 else 2 if b < 0xE0 else 3 if b < 0xF0 else 4
            cmap.setdefault(ord(d[pos:pos + l].decode('utf-8')), i); pos += l

def row(cp):
    g = glyph(cmap[cp])
    return '    {' + ','.join('0x%02X' % b for b in g) + '},'

print('#include "font8x16.h"\n\n/* Сгенерировано tools/gen_cyr_font.py */\n')
print('const unsigned char font_cyr[64][16] = {')
for cp in range(0x410, 0x450): print(row(cp))
print('};\n\nconst unsigned char font_yo[2][16] = {')
print(row(0x401)); print(row(0x451)); print('};')
