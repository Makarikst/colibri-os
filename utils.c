/* ============================================
   Colibri OS — утилиты (utils.c)
   ============================================ */

#include "utils.h"

/* Внешние функции ядра (VGA, RTC) */
extern void print(const char* str);
extern void putchar(char c);
extern void print_color(const char* str, unsigned char col);

#define LIGHT_RED    12
#define LIGHT_YELLOW 14
#define WHITE        15

/* ============================================
   Память
   ============================================ */
void memcpy(void* dst, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*) dst;
    const uint8_t* s = (const uint8_t*) src;
    while (n--) *d++ = *s++;
}

void memset(void* dst, uint8_t val, uint32_t n) {
    uint8_t* d = (uint8_t*) dst;
    while (n--) *d++ = val;
}

int memcmp(const void* a, const void* b, uint32_t n) {
    const uint8_t* x = (const uint8_t*) a;
    const uint8_t* y = (const uint8_t*) b;
    while (n--) {
        if (*x != *y) return *x - *y;
        x++; y++;
    }
    return 0;
}

void memmove(void* dst, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*) dst;
    const uint8_t* s = (const uint8_t*) src;
    if (d == s || n == 0) return;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
}

/* ============================================
   Строки
   ============================================ */
uint32_t strlen(const char* s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char* a, const char* b, uint32_t n) {
    while (n-- && *a && *b && *a == *b) { a++; b++; }
    if (n == (uint32_t)-1) return 0;
    return *a - *b;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while (*src) *d++ = *src++;
    *d = 0;
    return dst;
}

char* strncpy(char* dst, const char* src, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return dst;
}

char* strcat(char* dst, const char* src) {
    char* d = dst;
    while (*d) d++;
    while (*src) *d++ = *src++;
    *d = 0;
    return dst;
}

char* strncat(char* dst, const char* src, uint32_t n) {
    char* d = dst;
    while (*d) d++;
    while (n-- && *src) *d++ = *src++;
    *d = 0;
    return dst;
}

char* strchr(const char* s, char c) {
    while (*s) { if (*s == c) return (char*) s; s++; }
    return 0;
}

char* strrchr(const char* s, char c) {
    const char* last = 0;
    while (*s) { if (*s == c) last = s; s++; }
    return (char*) last;
}

char* strstr(const char* hay, const char* needle) {
    if (!*needle) return (char*) hay;
    while (*hay) {
        const char* h = hay;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*) hay;
        hay++;
    }
    return 0;
}

int starts_with(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++; prefix++;
    }
    return 1;
}

int ends_with(const char* s, const char* suffix) {
    uint32_t sl = strlen(s), pl = strlen(suffix);
    if (pl > sl) return 0;
    return strcmp(s + sl - pl, suffix) == 0;
}

void ltrim(char* s) {
    char* p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

void rtrim(char* s) {
    int n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;
}

void trim(char* s) { ltrim(s); rtrim(s); }

/* ============================================
   Числа
   ============================================ */
int atoi(const char* s) {
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return n * sign;
}

int strtol(const char* s, int base) {
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        n = n * base + d;
        s++;
    }
    return n * sign;
}

uint32_t itoa(int value, char* buf, int base) {
    char tmp[36];
    int i = 0, neg = 0;
    uint32_t uval;
    if (value < 0 && base == 10) { neg = 1; uval = (uint32_t)(-value); }
    else uval = (uint32_t) value;
    if (uval == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    while (uval) {
        int d = uval % base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        uval /= base;
    }
    uint32_t pos = 0;
    if (neg) buf[pos++] = '-';
    while (i > 0) buf[pos++] = tmp[--i];
    buf[pos] = 0;
    return pos;
}

int abs_int(int x) { return x < 0 ? -x : x; }
int min_int(int a, int b) { return a < b ? a : b; }
int max_int(int a, int b) { return a > b ? a : b; }
int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int pow_int(int base, int exp) {
    int r = 1;
    while (exp-- > 0) r *= base;
    return r;
}

int sqrt_int(int n) {
    if (n <= 0) return 0;
    int r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

int gcd(int a, int b) {
    a = abs_int(a); b = abs_int(b);
    while (b) { int t = b; b = a % b; a = t; }
    return a;
}

int lcm(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return abs_int(a * b) / gcd(a, b);
}

int is_prime(int n) {
    if (n < 2) return 0;
    if (n < 4) return 1;
    if (n % 2 == 0) return 0;
    for (int i = 3; i * i <= n; i += 2)
        if (n % i == 0) return 0;
    return 1;
}

/* ============================================
   RTC
   ============================================ */
static int bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t rtc_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

rtc_time_t rtc_get_time() {
    rtc_time_t t;
    t.sec  = bcd_to_bin(rtc_read(0x00));
    t.min  = bcd_to_bin(rtc_read(0x02));
    t.hour = bcd_to_bin(rtc_read(0x04));
    t.day  = bcd_to_bin(rtc_read(0x07));
    t.mon  = bcd_to_bin(rtc_read(0x08));
    t.year = 2000 + bcd_to_bin(rtc_read(0x09));
    return t;
}

uint32_t rtc_get_unix() {
    rtc_time_t t = rtc_get_time();
    /* Защита от мусора в RTC */
    if (t.year < 2000 || t.year > 2100) return 0;
    if (t.mon < 1 || t.mon > 12) return 0;
    if (t.day < 1 || t.day > 31) return 0;
    if (t.hour > 23) return 0;
    if (t.min > 59) return 0;
    if (t.sec > 59) return 0;

    uint32_t days = 0;
    for (int y = 2000; y < t.year; y++)
        days += ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) ? 366 : 365;
    int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (int m = 0; m < t.mon - 1; m++) {
        days += mdays[m];
        if (m == 1 && ((t.year % 4 == 0 && t.year % 100 != 0) || t.year % 400 == 0)) days++;
    }
    days += t.day - 1;
    return days * 86400u + t.hour * 3600u + t.min * 60u + t.sec;
}

/* ============================================
   Порты
   ============================================ */
void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ============================================
   Псевдографика (через внешние print/putchar)
   ============================================ */
void draw_hline(int x, int y, int len, char ch) {
    /* Не имеем прямого доступа к VGA — пропускаем.
       Реализация — через putchar_at, если понадобится. */
    (void)x; (void)y; (void)len; (void)ch;
}

void draw_vline(int x, int y, int len, char ch) {
    (void)x; (void)y; (void)len; (void)ch;
}

void draw_box(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
}

/* ============================================
   Логи
   ============================================ */
void klog(const char* level, const char* msg) {
    print("[");
    if (strcmp(level, "INFO") == 0)      print_color("INFO", 10);
    else if (strcmp(level, "WARN") == 0) print_color("WARN", LIGHT_YELLOW);
    else if (strcmp(level, "ERR") == 0)  print_color("ERR", LIGHT_RED);
    else print(level);
    print("] ");
    print(msg);
    putchar('\n');
}

void kpanic(const char* msg) {
    print_color("\n*** KPANIC ***\n", LIGHT_RED);
    print_color(msg, LIGHT_RED);
    putchar('\n');
    cpu_cli();
    while (1) cpu_halt();
}

void dump_hex(const void* data, uint32_t n) {
    const uint8_t* p = (const uint8_t*) data;
    char buf[4];
    for (uint32_t i = 0; i < n; i++) {
        itoa(p[i], buf, 16);
        if (p[i] < 16) putchar('0');
        print(buf);
        putchar(' ');
        if ((i + 1) % 16 == 0) putchar('\n');
    }
    putchar('\n');
}

/* ============================================
   RNG
   ============================================ */
static uint32_t rng_state = 0;

void srand(uint32_t seed) { rng_state = seed ? seed : 1; }

uint32_t rand_u32() {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

int rand_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rand_u32() % (uint32_t)(hi - lo + 1));
}

/* ============================================
   CPU
   ============================================ */
void cpu_halt() { __asm__ volatile ("hlt"); }
void cpu_cli()  { __asm__ volatile ("cli"); }
void cpu_sti()  { __asm__ volatile ("sti"); }