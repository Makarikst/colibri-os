ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text ALIGN(4K) :
    {
        *(.multiboot)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    /* ============================================
       DMA-буферы в первых 2 МБ — обязательны для RTL8139
       (карта не умеет работать с адресами выше 16 МБ,
        а часто и выше 4 МБ)
       ============================================ */
    .dma ALIGN(4K) :
    {
        _dma_start = .;
        *(.dma)
        *(.dma.*)
        _dma_end = .;
    }

    .bss ALIGN(4K) :
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }
}