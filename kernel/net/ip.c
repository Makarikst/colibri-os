#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "tcp.h"
#include "udp.h"

static uint16_t ip_id_counter = 1;

void ip_init(void) {
    ip_id_counter = 1;
}

/* ============================================
   Checksum (RFC 1071)
   ============================================ */
uint16_t ip_checksum(const void* data, uint32_t len) {
    const uint16_t* p = (const uint16_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t*)p;
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* ============================================
   Отправка IP-пакета
   ============================================ */
int ip_send(uint32_t dst_ip, uint8_t proto, const void* payload, uint32_t len) {
    /* 1. Узнаём MAC следующего хопа через ARP */
    uint32_t next_hop = dst_ip;
    /* Если адрес не в нашей подсети — шлём на шлюз */
    if ((dst_ip & NET_MASK) != (NET_IP & NET_MASK)) {
        next_hop = NET_GW;
    }

    uint8_t dst_mac[6];
    if (!arp_lookup(next_hop, dst_mac)) {
        /* Нет в кеше — шлём ARP-запрос и выходим (попробуй позже) */
        arp_request(next_hop);
        return 0;
    }

    /* 2. Собираем IP-заголовок */
    static uint8_t packet[20 + 1500];
    ip_header_t* ip = (ip_header_t*)packet;

    ip->ver_ihl    = 0x45;
    ip->tos        = 0;
    ip->total_len  = htons(20 + len);
    ip->id         = htons(ip_id_counter++);
    ip->flags_frag = htons(0x4000);   /* Don't Fragment */
    ip->ttl        = 64;
    ip->proto      = proto;
    ip->checksum   = 0;
    ip->src_ip     = htonl(NET_IP);
    ip->dst_ip     = htonl(dst_ip);
    ip->checksum   = ip_checksum(ip, 20);

    memcpy(packet + 20, payload, len);

    return net_send_ethernet(dst_mac, ETH_TYPE_IP, packet, 20 + len);
}

/* ============================================
   Обработка входящих IP
   ============================================ */
void ip_handle(const uint8_t* payload, uint32_t len) {
    if (len < 20) return;

    const ip_header_t* ip = (const ip_header_t*)payload;

    if ((ip->ver_ihl >> 4) != 4) return;

    uint32_t ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ihl < 20 || ihl > len) return;

    uint16_t total_len = ntohs(ip->total_len);
    if (total_len > len) total_len = len;

    const uint8_t* inner = payload + ihl;
    uint32_t inner_len = total_len - ihl;

    switch (ip->proto) {
        case IP_PROTO_ICMP:
            icmp_handle(inner, inner_len);
            break;
        case IP_PROTO_TCP:
            tcp_handle(inner, inner_len);
            break;
        case IP_PROTO_UDP:
            udp_handle(inner, inner_len);
            break;
        default:
            break;
    }
}