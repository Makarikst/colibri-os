#include "e1000.h"
#include "../vga/vga.h"

/* ============================================
   Состояние драйвера
   ============================================ */
static uint32_t e1000_mmio = 0;
static uint8_t  e1000_mac[6];
static int      e1000_ready = 0;

/* Счётчики (переименованы, чтобы не конфликтовать с функциями) */
static uint32_t e1000_rx_total = 0;
static uint32_t e1000_tx_total = 0;

/* RX/TX кольца и буферы в .dma секции */
static e1000_rx_desc_t rx_descs[E1000_NUM_RX_DESC]
    __attribute__((section(".dma"), aligned(16)));
static e1000_tx_desc_t tx_descs[E1000_NUM_TX_DESC]
    __attribute__((section(".dma"), aligned(16)));
static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_RX_BUF_SIZE]
    __attribute__((section(".dma"), aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][2048]
    __attribute__((section(".dma"), aligned(16)));

static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;

void (*e1000_on_packet)(const void* data, uint32_t len) = 0;

/* ============================================
   MMIO
   ============================================ */
static inline void e1000_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(e1000_mmio + reg) = value;
}

static inline uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t*)(e1000_mmio + reg);
}

/* ============================================
   Инициализация
   ============================================ */
int e1000_init(uint32_t bar0) {
    e1000_mmio = bar0 & 0xFFFFFFF0;
    e1000_ready = 0;
    e1000_rx_total = 0;
    e1000_tx_total = 0;

    vga_print("[E1000] mmio=0x");
    vga_print_hex32(e1000_mmio);
    vga_putchar('\n');

    /* 1. Reset */
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
    for (volatile int i = 0; i < 1000000; i++) { }

    /* 2. Маскируем все прерывания */
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    /* 3. Сброс счётчика прерываний */
    e1000_read(E1000_ICR);

    /* 4. Читаем MAC */
    uint32_t ral = e1000_read(E1000_RAL);
    uint32_t rah = e1000_read(E1000_RAH);
    e1000_mac[0] = ral & 0xFF;
    e1000_mac[1] = (ral >> 8) & 0xFF;
    e1000_mac[2] = (ral >> 16) & 0xFF;
    e1000_mac[3] = (ral >> 24) & 0xFF;
    e1000_mac[4] = rah & 0xFF;
    e1000_mac[5] = (rah >> 8) & 0xFF;

    vga_print("[E1000] MAC=");
    for (int i = 0; i < 6; i++) {
        char hex[] = "0123456789abcdef";
        vga_putchar(hex[e1000_mac[i] >> 4]);
        vga_putchar(hex[e1000_mac[i] & 0xF]);
        if (i < 5) vga_putchar(':');
    }
    vga_print("\n");

    /* 5. Очищаем multicast-таблицу */
    for (int i = 0; i < 128; i++) e1000_write(E1000_MTA + i * 4, 0);

    /* 6. Линк: up, full duplex */
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU | E1000_CTRL_FD);

    /* 7. RX-дескрипторы */
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr   = (uint64_t)(uint32_t)rx_buffers[i];
        rx_descs[i].status = 0;
    }

    e1000_write(E1000_RDBAL, (uint32_t)rx_descs);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);
    rx_cur = 0;

    /* 8. RCTL */
    e1000_write(E1000_RCTL,
        E1000_RCTL_EN |
        E1000_RCTL_SBP |
        E1000_RCTL_UPE |
        E1000_RCTL_MPE |
        E1000_RCTL_BAM |
        E1000_RCTL_SECRC |
        E1000_RCTL_BSIZE_8192);

    /* 9. TX-дескрипторы */
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr   = (uint64_t)(uint32_t)tx_buffers[i];
        tx_descs[i].length = 0;
        tx_descs[i].cmd    = 0;
        tx_descs[i].status = 0;
    }

    e1000_write(E1000_TDBAL, (uint32_t)tx_descs);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    tx_cur = 0;

    /* 10. TCTL */
    e1000_write(E1000_TCTL,
        E1000_TCTL_EN |
        E1000_TCTL_PSP |
        (0x10 << E1000_TCTL_CT_SHIFT) |
        (0x40 << E1000_TCTL_COLD_SHIFT));

    /* 11. TIPG */
    e1000_write(E1000_TIPG, 10 | (8 << 10) | (6 << 20));

    /* Отладка */
    vga_print("[E1000] CTRL=0x");
    vga_print_hex32(e1000_read(E1000_CTRL));
    vga_print(" STATUS=0x");
    vga_print_hex32(e1000_read(E1000_STATUS));
    vga_print(" RCTL=0x");
    vga_print_hex32(e1000_read(E1000_RCTL));
    vga_print(" TCTL=0x");
    vga_print_hex32(e1000_read(E1000_TCTL));
    vga_putchar('\n');
    vga_print("[E1000] RDBAL=0x");
    vga_print_hex32(e1000_read(E1000_RDBAL));
    vga_print(" TDBAL=0x");
    vga_print_hex32(e1000_read(E1000_TDBAL));
    vga_putchar('\n');

    e1000_ready = 1;
    return 1;
}

void e1000_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = e1000_mac[i];
}

/* ============================================
   Отправка
   ============================================ */
int e1000_send(const void* packet, uint32_t len) {
    if (!e1000_ready) return 0;
    if (len > 2048) return 0;

    uint32_t slot = tx_cur;

    int timeout = 1000000;
    while (!(tx_descs[slot].status & 0x01) && tx_descs[slot].cmd != 0) {
        if (timeout-- <= 0) {
            vga_print_color("[E1000] TX timeout\n", VGA_LIGHT_RED);
            return 0;
        }
    }

    memcpy(tx_buffers[slot], packet, len);
    if (len < 60) {
        memset(tx_buffers[slot] + len, 0, 60 - len);
        len = 60;
    }

    tx_descs[slot].addr   = (uint64_t)(uint32_t)tx_buffers[slot];
    tx_descs[slot].length = len;
    tx_descs[slot].cmd    = 0x0B;   /* EOP | IFCS | RS */
    tx_descs[slot].status = 0;

    tx_cur = (slot + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_TDT, tx_cur);

    e1000_tx_total++;
    return 1;
}

/* ============================================
   Приём
   ============================================ */
int e1000_poll(void) {
    if (!e1000_ready) return 0;

    int got = 0;
    int safety = 32;

    while (safety-- > 0) {
        e1000_rx_desc_t* d = &rx_descs[rx_cur];
        if (!(d->status & 0x01)) break;

        uint32_t pkt_len = d->length;
        if (pkt_len > 0 && pkt_len <= E1000_RX_BUF_SIZE) {
            e1000_rx_total++;
            if (e1000_on_packet) {
                e1000_on_packet(rx_buffers[rx_cur], pkt_len);
            }
            got = 1;
        }

        d->status = 0;
        e1000_write(E1000_RDT, rx_cur);
        rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    }

    return got;
}

uint32_t e1000_tx_count(void) { return e1000_tx_total; }
uint32_t e1000_rx_count(void) { return e1000_rx_total; }