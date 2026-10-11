#include "rtl8139.h"
#include "../vga/vga.h"

static uint32_t rtl_io       = 0;
static uint8_t  rtl_mac[6]   = {0};
static int      rtl_ready    = 0;

static uint8_t  rtl_rx_buffer[RTL_RX_BUF_SIZE + 16] __attribute__((aligned(4)));
static uint32_t rtl_rx_offset = 0;

static uint8_t  rtl_tx_buffers[RTL_TX_SLOTS][RTL_TX_BUF_SIZE] __attribute__((aligned(256)));
static int      rtl_tx_next   = 0;

static uint32_t rtl_rx_len    = 0;
static uint32_t rtl_tx_count  = 0;
static uint32_t rtl_rx_count  = 0;

void (*rtl8139_on_packet)(const void* data, uint32_t len) = 0;

static inline void rtl_outb(uint16_t off, uint8_t v)  { outb(rtl_io + off, v); }
static inline void rtl_outw(uint16_t off, uint16_t v) { outw(rtl_io + off, v); }
static inline void rtl_outl(uint16_t off, uint32_t v) { outl(rtl_io + off, v); }
static inline uint8_t  rtl_inb(uint16_t off)  { return inb(rtl_io + off); }
static inline uint16_t rtl_inw(uint16_t off)  { return inw(rtl_io + off); }
static inline uint32_t rtl_inl(uint16_t off)  { return inl(rtl_io + off); }

/* ============================================
   Инициализация
   ============================================ */
int rtl8139_init(uint32_t bar0) {
    uint32_t io_base = bar0 & 0xFFFC;

    rtl_io    = io_base;
    rtl_ready = 0;
    rtl_rx_offset = 0;
    rtl_tx_next   = 0;
    rtl_tx_count  = 0;
    rtl_rx_count  = 0;

    vga_print("[RTL] bar0=0x");
    vga_print_hex32(bar0);
    vga_print(" io_base=0x");
    vga_print_hex32(io_base);
    vga_putchar('\n');

    /* 1. Power on */
    rtl_outb(RTL_CONFIG1, 0x00);

    /* 2. Reset */
    rtl_outb(RTL_CMD, RTL_CMD_RST);
    int timeout = 100000;
    while ((rtl_inb(RTL_CMD) & RTL_CMD_RST) && timeout-- > 0) { }
    if (timeout <= 0) {
        vga_print_color("[RTL] reset timeout\n", VGA_LIGHT_RED);
        return 0;
    }

    /* 3. Прочитать MAC */
    for (int i = 0; i < 6; i++)
        rtl_mac[i] = rtl_inb(RTL_IDR0 + i);

    /* 4. Установить RBSTART (адрес RX буфера, должен быть 4-байт выровнен) */
    rtl_outl(RTL_RBSTART, (uint32_t)rtl_rx_buffer);

    rtl_outw(RTL_IMR, 0x0005);   /* ROK (RX OK) | TOK (TX OK) */

    /* 6. Сбросить ISR (write-1-to-clear) */
    rtl_outw(RTL_ISR, 0xFFFF);

    /* 7. RCR:
        bit 0 (AB):  accept broadcast
        bit 1 (AM):  accept multicast
        bit 2 (APM): accept physical match
        bit 3 (AAP): accept all packets (promiscuous)
        bit 7 (WRAP): wrap-around ring buffer
        bits 8-10: MXDMA = 6 (1024 bytes) — важно!
        bits 11-12: RBLEN = 0 (8K)
    */
    uint32_t rcr = 0x0000000F | (1 << 7) | (6 << 8);
    rtl_outl(RTL_RCR, rcr);

    /* 8. TCR (256-байтные порции, стандарт) */
    rtl_outl(RTL_TCR, 0x03000700);

    /* 9. Включить RX + TX */
    rtl_outb(RTL_CMD, RTL_CMD_RE | RTL_CMD_TE);

    /* 10. CAPR указывает на "предыдущий прочитанный" — ставим в -16 */
    rtl_outw(RTL_CAPR, 0x0000);

    rtl_ready = 1;

    /* Отладка */
    vga_print("[RTL] MAC=");
    for (int i = 0; i < 6; i++) {
        char hex[] = "0123456789abcdef";
        vga_putchar(hex[rtl_mac[i] >> 4]);
        vga_putchar(hex[rtl_mac[i] & 0xF]);
        if (i < 5) vga_putchar(':');
    }
    vga_print("\n[RTL] CMD=0x");
    vga_print_hex32(rtl_inb(RTL_CMD));
    vga_print(" RCR=0x");
    vga_print_hex32(rtl_inl(RTL_RCR));
    vga_print(" ISR=0x");
    vga_print_hex32(rtl_inw(RTL_ISR));
    vga_print("\n[RTL] RBSTART=0x");
    vga_print_hex32(rtl_inl(RTL_RBSTART));
    vga_print(" CBR=0x");
    vga_print_hex32(rtl_inw(RTL_CBR));
    vga_print(" CAPR=0x");
    vga_print_hex32(rtl_inw(RTL_CAPR));
    vga_putchar('\n');

    return 1;
}

void rtl8139_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = rtl_mac[i];
}

/* ============================================
   Отправка
   ============================================ */
int rtl8139_send(const void* packet, uint32_t len) {
    if (!rtl_ready) return 0;
    if (len < 14 || len > RTL_TX_BUF_SIZE) return 0;

    int slot = rtl_tx_next;

    int timeout = 1000000;
    while ((rtl_inl(RTL_TSD0 + slot * 4) & 0x8000) && timeout-- > 0) { }
    if (timeout <= 0) return 0;

    memcpy(rtl_tx_buffers[slot], packet, len);
    uint32_t send_len = len;
    if (send_len < 60) {
        memset(rtl_tx_buffers[slot] + send_len, 0, 60 - send_len);
        send_len = 60;
    }

    uint32_t phys_addr = (uint32_t)rtl_tx_buffers[slot];

    /* Отладка: смотрим, что мы шлём */
    vga_print("[RTL] TX slot=");
    vga_print_dec(slot);
    vga_print(" addr=0x");
    vga_print_hex32(phys_addr);
    vga_print(" len=");
    vga_print_dec(send_len);
    vga_putchar('\n');

    rtl_outl(RTL_TSAD0 + slot * 4, phys_addr);
    rtl_outl(RTL_TSD0 + slot * 4, (send_len & 0x1FFF) | 0x2000);
    /* Проверим, что карта забрала пакет (бит OWN должен сброситься в 0) */
    int t = 10000;
    while (t-- > 0) { }
    uint32_t tsd_after = rtl_inl(RTL_TSD0 + slot * 4);
    vga_print("[RTL] TSD after=0x");
    vga_print_hex32(tsd_after);
    vga_putchar('\n');

    rtl_tx_next = (rtl_tx_next + 1) % RTL_TX_SLOTS;
    rtl_tx_count++;
    return 1;
}

/* ============================================
   Приём — polling
   ============================================ */
int rtl8139_poll(void) {
    if (!rtl_ready) return 0;

    uint16_t isr = rtl_inw(RTL_ISR);
    if (isr) rtl_outw(RTL_ISR, isr);

    /* Проверяем наличие данных иначе: через CAPR vs CBR */
    uint16_t cbr  = rtl_inw(RTL_CBR);
    uint16_t capr = rtl_inw(RTL_CAPR);

    /* Если cbr == capr+16 — буфер пуст (нет новых пакетов) */
    if (((cbr - capr) & 0xFFFF) <= 16) {
        return 0;
    }

    /* Читаем заголовок */
    uint16_t status = *(volatile uint16_t*)(rtl_rx_buffer + rtl_rx_offset);
    uint16_t length = *(volatile uint16_t*)(rtl_rx_buffer + rtl_rx_offset + 2);

    if (length < 4 || length > RTL_RX_BUF_SIZE) {
        vga_print_color("[RTL] bad len=", VGA_LIGHT_RED);
        vga_print_dec(length);
        vga_print(" status=0x");
        vga_print_hex32(status);
        vga_print(" cbr=0x");
        vga_print_hex32(cbr);
        vga_print(" capr=0x");
        vga_print_hex32(capr);
        vga_putchar('\n');

        /* Сброс: просто переставим CAPR */
        rtl_outw(RTL_CAPR, 0xFFF0);
        rtl_rx_offset = 0;
        return 0;
    }

    uint32_t pkt_len = length - 4;
    rtl_rx_len = pkt_len;
    rtl_rx_count++;

    const uint8_t* d = rtl_rx_buffer + rtl_rx_offset + 4;

    if (pkt_len >= 14) {
        uint16_t et = (d[12] << 8) | d[13];
        vga_print("[RTL] RX #");
        vga_print_dec(rtl_rx_count);
        vga_print(" len=");
        vga_print_dec(pkt_len);
        vga_print(" et=0x");
        vga_print_hex32(et);
        vga_putchar('\n');
    }

    if (rtl8139_on_packet) {
        rtl8139_on_packet(d, pkt_len);
    }

    rtl_rx_offset = (rtl_rx_offset + length + 4 + 3) & ~3;
    rtl_rx_offset %= RTL_RX_BUF_SIZE;

    /* CAPR = offset - 16 */
    rtl_outw(RTL_CAPR, (uint16_t)(rtl_rx_offset - 16));

    return 1;
}

const void* rtl8139_rx_buffer(void) { return rtl_rx_buffer; }
uint32_t    rtl8139_rx_length(void) { return rtl_rx_len; }
void        rtl8139_rx_consume(void) { rtl_rx_len = 0; }

uint32_t rtl8139_tx_count(void) { return rtl_tx_count; }
uint32_t rtl8139_rx_count(void) { return rtl_rx_count; }