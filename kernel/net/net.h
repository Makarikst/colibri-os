#ifndef NET_H
#define NET_H

#include "../utils/utils.h"

/* ============================================
   Общий сетевой слой
   ============================================ */

/* Сетевые настройки (QEMU user-mode) */
#define NET_IP      0x0A00020F   /* 10.0.2.15  — наш адрес */
#define NET_MASK    0xFFFFFF00   /* 255.255.255.0 */
#define NET_GW      0x0A000202   /* 10.0.2.2   — шлюз */
#define NET_DNS     0x0A000203   /* 10.0.2.3   — DNS */

/* Порядок байт */
static inline uint16_t htons(uint16_t x) { return (x >> 8) | (x << 8); }
static inline uint16_t ntohs(uint16_t x) { return (x >> 8) | (x << 8); }
static inline uint32_t htonl(uint32_t x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
}
static inline uint32_t ntohl(uint32_t x) { return htonl(x); }

/* Протоколы EtherType */
#define ETH_TYPE_IP   0x0800
#define ETH_TYPE_ARP  0x0806

/* IP-протоколы */
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

/* MAC-адрес */
typedef struct { uint8_t b[6]; } mac_addr_t;

/* Инициализация — вызывается из kernel_main после rtl8139_init */
void net_init(void);

/* Вызывается из главного цикла — принимает пакеты и раздаёт по стекам */
void net_poll(void);

/* Наш MAC */
void net_get_mac(uint8_t mac[6]);

/* Отправка готового Ethernet-фрейма (dst_mac уже заполнен) */
int  net_send_ethernet(const uint8_t dst_mac[6], uint16_t ether_type,
                       const void* payload, uint32_t len);

#endif