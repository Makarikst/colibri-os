#include "udp.h"
#include "ip.h"
#include "dns.h"

void udp_init(void) { }

/* ============================================
   Отправка
   ============================================ */
int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
             const void* data, uint32_t len)
{
    static uint8_t buf[8 + 1500];
    udp_header_t* h = (udp_header_t*)buf;

    h->src_port = htons(src_port);
    h->dst_port = htons(dst_port);
    h->length   = htons(8 + len);
    h->checksum = 0;   /* для IPv4 UDP checksum опционален; 0 = "не проверять" */

    if (len > 1500) len = 1500;
    memcpy(buf + 8, data, len);

    return ip_send(dst_ip, IP_PROTO_UDP, buf, 8 + len);
}

/* ============================================
   Приём
   ============================================ */
void udp_handle(const uint8_t* payload, uint32_t len) {
    if (len < 8) return;
    const udp_header_t* h = (const udp_header_t*)payload;
    uint16_t dst_port = ntohs(h->dst_port);
    uint16_t src_port = ntohs(h->src_port);
    uint16_t udp_len  = ntohs(h->length);
    if (udp_len < 8 || udp_len > len) udp_len = len;

    const uint8_t* data = payload + 8;
    uint32_t data_len = udp_len - 8;

    /* DNS отдаём резолверу */
    if (src_port == 53) {
        dns_handle(data, data_len);
    }
}