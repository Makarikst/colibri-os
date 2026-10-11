#include "net.h"
#include "e1000.h"
#include "ethernet.h"
#include "arp.h"
#include "ip.h"
#include "tcp.h"
#include "udp.h"
#include "dns.h"
#include "../vga/vga.h"

static uint8_t our_mac[6];
static int     net_ready = 0;

void net_init(void) {
    e1000_get_mac(our_mac);
    arp_init();
    ip_init();
    tcp_init();
    udp_init();
    dns_init();
    net_ready = 1;
    vga_print("NET: ready, IP=10.0.2.15 gw=10.0.2.2 dns=10.0.2.3\n");
}

void net_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = our_mac[i];
}

int net_send_ethernet(const uint8_t dst_mac[6], uint16_t ether_type,
                      const void* payload, uint32_t len)
{
    if (!net_ready) return 0;
    if (len > 1500) return 0;

    static uint8_t frame[14 + 1500];
    for (int i = 0; i < 6; i++) frame[i]     = dst_mac[i];
    for (int i = 0; i < 6; i++) frame[6 + i] = our_mac[i];
    frame[12] = (ether_type >> 8) & 0xFF;
    frame[13] = ether_type & 0xFF;
    memcpy(frame + 14, payload, len);

    return e1000_send(frame, 14 + len);
}

static void net_handle_packet(const void* data, uint32_t len) {
    if (len < 14) return;
    const uint8_t* p = (const uint8_t*)data;
    uint16_t ether_type = (p[12] << 8) | p[13];

    switch (ether_type) {
        case ETH_TYPE_ARP:  arp_handle(p + 14, len - 14); break;
        case ETH_TYPE_IP:   ip_handle(p + 14, len - 14);  break;
        default: break;
    }
}

void net_poll(void) {
    if (!net_ready) return;
    e1000_on_packet = net_handle_packet;
    e1000_poll();
}