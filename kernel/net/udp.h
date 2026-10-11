#ifndef UDP_H
#define UDP_H

#include "net.h"

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

void udp_init(void);
void udp_handle(const uint8_t* payload, uint32_t len);

/* Отправить UDP-датаграмму */
int  udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void* data, uint32_t len);

#endif