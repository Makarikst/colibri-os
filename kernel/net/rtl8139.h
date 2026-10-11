#ifndef RTL8139_H
#define RTL8139_H

#include "../utils/utils.h"

#define RTL_VENDOR_ID  0x10EC
#define RTL_DEVICE_ID  0x8139

#define RTL_IDR0        0x00
#define RTL_TSD0        0x10
#define RTL_TSAD0       0x20
#define RTL_RBSTART     0x30
#define RTL_CMD         0x37
#define RTL_CAPR        0x38
#define RTL_CBR         0x3A
#define RTL_IMR         0x3C
#define RTL_ISR         0x3E
#define RTL_TCR         0x40
#define RTL_RCR         0x44
#define RTL_9346CR      0x50
#define RTL_CONFIG1     0x52

#define RTL_CMD_RST     0x10
#define RTL_CMD_RE      0x08
#define RTL_CMD_TE      0x04
#define RTL_CMD_BUFE    0x01

#define RTL_ISR_ROK     0x0001
#define RTL_ISR_TOK     0x0004
#define RTL_ISR_RER     0x0002
#define RTL_ISR_TER     0x0008

#define RTL_RX_BUF_SIZE  8192
#define RTL_TX_BUF_SIZE  1536
#define RTL_TX_SLOTS     4

int  rtl8139_init(uint32_t io_base);
void rtl8139_get_mac(uint8_t mac[6]);
int  rtl8139_send(const void* packet, uint32_t len);
int  rtl8139_poll(void);
const void* rtl8139_rx_buffer(void);
uint32_t    rtl8139_rx_length(void);
void        rtl8139_rx_consume(void);

uint32_t rtl8139_tx_count(void);
uint32_t rtl8139_rx_count(void);

extern void (*rtl8139_on_packet)(const void* data, uint32_t len);

#endif