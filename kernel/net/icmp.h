#ifndef ICMP_H
#define ICMP_H

#include "net.h"

typedef struct __attribute__((packed)) {
    uint8_t  type;      /* 8 = echo request, 0 = echo reply */
    uint8_t  code;      /* 0 */
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_header_t;

void icmp_init(void);
void icmp_handle(const uint8_t* payload, uint32_t len);

/* Отправить ping. Возвращает 1, если успешно отправлен. */
int icmp_ping(uint32_t dst_ip, uint16_t id, uint16_t seq);

#endif