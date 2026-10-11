#ifndef ETHERNET_H
#define ETHERNET_H

#include "net.h"

/* Заголовок Ethernet II — 14 байт */
typedef struct __attribute__((packed)) {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ether_type;   /* big-endian */
} eth_header_t;

#endif