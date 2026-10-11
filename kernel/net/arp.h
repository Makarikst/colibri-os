#ifndef ARP_H
#define ARP_H

#include "net.h"

#define ARP_TABLE_SIZE  16

typedef struct __attribute__((packed)) {
    uint16_t htype;      /* 1 = Ethernet */
    uint16_t ptype;      /* 0x0800 = IPv4 */
    uint8_t  hlen;       /* 6 */
    uint8_t  plen;       /* 4 */
    uint16_t opcode;     /* 1 = request, 2 = reply */
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} arp_packet_t;

void arp_init(void);
void arp_handle(const uint8_t* payload, uint32_t len);

/* Отправить ARP-запрос — надо, чтобы узнать MAC по IP */
int  arp_request(uint32_t ip);

/* Найти MAC в кеше. Возвращает 1, если есть, иначе 0 */
int  arp_lookup(uint32_t ip, uint8_t mac_out[6]);

/* Для отладки: команда arp в терминале */
void arp_dump(void);

#endif