/* ============================================
   Colibri OS — утилиты (utils.h)
   ============================================ */

#ifndef UTILS_H
#define UTILS_H

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long long uint64_t;

/* ---------- Память ---------- */
void  memcpy(void* dst, const void* src, uint32_t n);
void  memset(void* dst, uint8_t val, uint32_t n);
int   memcmp(const void* a, const void* b, uint32_t n);
void  memmove(void* dst, const void* src, uint32_t n);

/* ---------- Строки ---------- */
uint32_t strlen(const char* s);
int      strcmp(const char* a, const char* b);
int      strncmp(const char* a, const char* b, uint32_t n);
char*    strcpy(char* dst, const char* src);
char*    strncpy(char* dst, const char* src, uint32_t n);
char*    strcat(char* dst, const char* src);
char*    strncat(char* dst, const char* src, uint32_t n);
char*    strchr(const char* s, char c);
char*    strrchr(const char* s, char c);
char*    strstr(const char* hay, const char* needle);
int      starts_with(const char* s, const char* prefix);
int      ends_with(const char* s, const char* suffix);
void     trim(char* s);
void     ltrim(char* s);
void     rtrim(char* s);

/* ---------- Числа ---------- */
int      atoi(const char* s);
int      strtol(const char* s, int base);
uint32_t itoa(int value, char* buf, int base);
int      abs_int(int x);
int      min_int(int a, int b);
int      max_int(int a, int b);
int      clamp_int(int v, int lo, int hi);
int      pow_int(int base, int exp);
int      sqrt_int(int n);
int      gcd(int a, int b);
int      lcm(int a, int b);
int      is_prime(int n);

/* ---------- RTC ---------- */
typedef struct {
    int sec, min, hour;
    int day, mon, year;
} rtc_time_t;

rtc_time_t rtc_get_time(void);
uint32_t   rtc_get_unix(void);

/* ---------- Клавиатура / ввод-вывод ---------- */
void     outb(uint16_t port, uint8_t val);
uint8_t  inb(uint16_t port);

/* ---------- Псевдографика ---------- */
void draw_box(int x, int y, int w, int h);
void draw_hline(int x, int y, int len, char ch);
void draw_vline(int x, int y, int len, char ch);

/* ---------- Логи / ошибки ---------- */
void klog(const char* level, const char* msg);
void kpanic(const char* msg);
void dump_hex(const void* data, uint32_t n);

/* ---------- Генератор случайных чисел ---------- */
void     srand(uint32_t seed);
uint32_t rand_u32(void);
int      rand_range(int lo, int hi);

/* ---------- CPU ---------- */
void cpu_halt(void);
void cpu_cli(void);
void cpu_sti(void);

#endif