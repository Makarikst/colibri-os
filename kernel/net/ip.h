#ifndef IP_H
#define IP_H

#include "net.h"

typedef struct __attribute__((packed)) {
    uint8_t  ver_ihl;       /* 0x45 = IPv4, 20 байт заголовок */
    uint8_t  tos;
    uint16_t total_len;     /* big-endian */
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint32_t src_ip;        /* big-endian */
    uint32_t dst_ip;        /* big-endian */
} ip_header_t;

void ip_init(void);
void ip_handle(const uint8_t* payload, uint32_t len);

/* Отправить IP-пакет (payload — TCP/UDP/ICMP) */
int  ip_send(uint32_t dst_ip, uint8_t proto, const void* payload, uint32_t len);

uint16_t ip_checksum(const void* data, uint32_t len);

#endif