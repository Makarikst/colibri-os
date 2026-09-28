#include "kmalloc.h"
#include "../vga/vga.h"

/* ============================================
   Начало и конец кучи
   ============================================ */
#define HEAP_START  0x400000    /* 4 МБ — выше .bss */
#define HEAP_SIZE   0x100000    /* 1 МБ */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

/* ============================================
   Заголовок блока
   ============================================ */
typedef struct block_header {
    unsigned int size;              /* размер блока (без заголовка) */
    int free;                       /* 1 = свободен, 0 = занят */
    struct block_header* next;      /* следующий блок */
} block_header_t;

static block_header_t* heap_start = 0;
static unsigned int heap_used = 0;

/* ============================================
   Инициализация
   ============================================ */
void heap_init(void) {
    heap_start = (block_header_t*) HEAP_START;
    heap_start->size = HEAP_SIZE - sizeof(block_header_t);
    heap_start->free = 1;
    heap_start->next = 0;
    heap_used = 0;
}

/* ============================================
   Выделение
   ============================================ */
void* kmalloc(unsigned int size) {
    if (size == 0) return 0;

    /* Выравниваем до 8 байт */
    size = (size + 7) & ~7;

    block_header_t* current = heap_start;

    while (current) {
        if (current->free && current->size >= size) {
            /* Если блок больше — разделим */
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

    return 0;   /* нет памяти */
}

/* ============================================
   Освобождение
   ============================================ */
void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* block =
        (block_header_t*)((char*)ptr - sizeof(block_header_t));

    block->free = 1;
    heap_used -= block->size;

    /* Объединяем со следующим, если свободен */
    if (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
    }
}

/* ============================================
   Статистика
   ============================================ */
void heap_stats(void) {
    vga_print("Heap start: ");
    vga_print_hex32((unsigned int)heap_start);
    vga_print("\nHeap size:  ");
    vga_print_dec(HEAP_SIZE);
    vga_print(" bytes\nHeap used:  ");
    vga_print_dec((unsigned int)heap_used);
    vga_print(" bytes\n");
}