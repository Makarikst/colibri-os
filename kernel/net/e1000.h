#ifndef E1000_H
#define E1000_H

#include "../utils/utils.h"

#define E1000_VENDOR_ID  0x8086
#define E1000_DEVICE_ID  0x100E   /* 82540EM — QEMU по умолчанию */

/* Регистры (смещения от MMIO BAR0) */
#define E1000_CTRL      0x0000
#define E1000_STATUS    0x0008
#define E1000_ICR       0x00C0
#define E1000_IMS       0x00D0
#define E1000_IMC       0x00D8
#define E1000_RCTL      0x0100
#define E1000_TCTL      0x0400
#define E1000_TIPG      0x0410
#define E1000_RDBAL     0x2800
#define E1000_RDBAH     0x2804
#define E1000_RDLEN     0x2808
#define E1000_RDH       0x2810
#define E1000_RDT       0x2818
#define E1000_TDBAL     0x3800
#define E1000_TDBAH     0x3804
#define E1000_TDLEN     0x3808
#define E1000_TDH       0x3810
#define E1000_TDT       0x3818
#define E1000_MTA       0x5200
#define E1000_RAL       0x5400
#define E1000_RAH       0x5404

/* CTRL биты */
#define E1000_CTRL_FD       (1 << 0)
#define E1000_CTRL_LRST     (1 << 3)
#define E1000_CTRL_ASDE     (1 << 5)
#define E1000_CTRL_SLU      (1 << 6)
#define E1000_CTRL_RST      (1 << 26)

/* RCTL биты */
#define E1000_RCTL_EN       (1 << 1)
#define E1000_RCTL_SBP      (1 << 2)
#define E1000_RCTL_UPE      (1 << 3)
#define E1000_RCTL_MPE      (1 << 4)
#define E1000_RCTL_BAM      (1 << 15)
#define E1000_RCTL_SECRC    (1 << 26)
#define E1000_RCTL_BSIZE_2048  0
#define E1000_RCTL_BSIZE_4096  (1 << 16)
#define E1000_RCTL_BSIZE_8192  (2 << 16)

/* TCTL биты */
#define E1000_TCTL_EN       (1 << 1)
#define E1000_TCTL_PSP      (1 << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12

/* Количество дескрипторов в кольцах */
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32
#define E1000_RX_BUF_SIZE 8192

/* Дескриптор RX */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

/* Дескриптор TX */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

/* Инициализация. mmio_base — BAR0 (физический адрес, выровненный по 16) */
int  e1000_init(uint32_t mmio_base);

/* Прочитать MAC */
void e1000_get_mac(uint8_t mac[6]);

/* Отправить Ethernet-фрейм */
int  e1000_send(const void* packet, uint32_t len);

/* Polling — если есть пакет, вызывает колбэк */
int  e1000_poll(void);

/* Счётчики для отладки */
uint32_t e1000_tx_count(void);
uint32_t e1000_rx_count(void);

/* Колбэк приёма */
extern void (*e1000_on_packet)(const void* data, uint32_t len);

#endif