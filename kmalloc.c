/* ============================================
   Colibri OS — управление памятью (куча)
   Простой bump-allocator с free-list
   ============================================ */

#include "kmalloc.h"

/* Начало и конец кучи в памяти */
#define HEAP_START  0x200000    /* 1 МБ */
#define HEAP_SIZE   0x100000    /* 1 МБ (до 2 МБ) */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

/* Заголовок блока */
typedef struct block_header {
    size_t size;                    /* размер блока (без заголовка) */
    int free;                       /* 1 = свободен, 0 = занят */
    struct block_header* next;      /* следующий блок */
} block_header_t;

static block_header_t* heap_start = 0;
static size_t heap_used = 0;

/* Инициализация кучи */
void heap_init() {
    heap_start = (block_header_t*) HEAP_START;
    heap_start->size = HEAP_SIZE - sizeof(block_header_t);
    heap_start->free = 1;
    heap_start->next = 0;
    heap_used = 0;
}

/* Выделение памяти */
void* kmalloc(size_t size) {
    if (size == 0) return 0;

    /* Выравниваем размер до 8 байт */
    size = (size + 7) & ~7;

    block_header_t* current = heap_start;

    while (current) {
        if (current->free && current->size >= size) {
            /* Нашли подходящий блок */

            /* Если блок больше, чем нужно — разделим */
            if (current->size > size + sizeof(block_header_t) + 8) {
                block_header_t* new_block =
                    (block_header_t*)((char*)current + sizeof(block_header_t) + size);

                new_block->size = current->size - size - sizeof(block_header_t);
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->free = 0;
            heap_used += current->size;
            return (void*)((char*)current + sizeof(block_header_t));
        }

        current = current->next;
    }

    /* Не хватило памяти */
    return 0;
}

/* Освобождение памяти */
void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* block =
        (block_header_t*)((char*)ptr - sizeof(block_header_t));

    block->free = 1;
    heap_used -= block->size;

    /* Объединяем со следующим блоком, если он свободен */
    if (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
    }
}

/* Статистика кучи — выводит информацию в VGA */
extern void print(const char* str);
extern void putchar(char c);

static void print_hex(unsigned int n) {
    char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex[(n >> (28 - i * 4)) & 0xF];
    }
    buf[10] = 0;
    print(buf);
}

static void print_dec(unsigned int n) {
    if (n == 0) {
        putchar('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) putchar(buf[--i]);
}

void heap_stats() {
    print("Heap start: ");
    print_hex((unsigned int)heap_start);
    print("\nHeap size:  ");
    print_dec(HEAP_SIZE);
    print(" bytes\nHeap used:  ");
    print_dec((unsigned int)heap_used);
    print(" bytes\n");
}