#include "arp.h"
#include "../vga/vga.h"

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_TABLE_SIZE];
static int arp_count = 0;

/* Broadcast MAC */
static const uint8_t bcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void arp_init(void) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) arp_cache[i].valid = 0;
    arp_count = 0;
}

/* ============================================
   Сохранить/найти запись
   ============================================ */
static void arp_store(uint32_t ip, const uint8_t mac[6]) {
    for (int i = 0; i < arp_count; i++) {
        if (arp_cache[i].ip == ip) {
            for (int j = 0; j < 6; j++) arp_cache[i].mac[j] = mac[j];
            arp_cache[i].valid = 1;
            return;
        }
    }
    if (arp_count < ARP_TABLE_SIZE) {
        arp_cache[arp_count].ip = ip;
        for (int j = 0; j < 6; j++) arp_cache[arp_count].mac[j] = mac[j];
        arp_cache[arp_count].valid = 1;
        arp_count++;
    }
}

int arp_lookup(uint32_t ip, uint8_t mac_out[6]) {
    for (int i = 0; i < arp_count; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            for (int j = 0; j < 6; j++) mac_out[j] = arp_cache[i].mac[j];
            return 1;
        }
    }
    return 0;
}

/* ============================================
   Отправка ARP-запроса
   ============================================ */
int arp_request(uint32_t ip) {
    arp_packet_t req;
    req.htype = htons(1);
    req.ptype = htons(ETH_TYPE_IP);
    req.hlen  = 6;
    req.plen  = 4;
    req.opcode = htons(1);      /* request */
    net_get_mac(req.sender_mac);
    req.sender_ip = htonl(NET_IP);
    for (int i = 0; i < 6; i++) req.target_mac[i] = 0;
    req.target_ip = htonl(ip);

    return net_send_ethernet(bcast_mac, ETH_TYPE_ARP, &req, sizeof(req));
}

/* ============================================
   Обработка входящих ARP
   ============================================ */
void arp_handle(const uint8_t* payload, uint32_t len) {
    if (len < sizeof(arp_packet_t)) return;

    const arp_packet_t* p = (const arp_packet_t*)payload;

    if (ntohs(p->htype) != 1) return;
    if (ntohs(p->ptype) != ETH_TYPE_IP) return;

    uint32_t sender_ip = ntohl(p->sender_ip);
    uint32_t target_ip = ntohl(p->target_ip);

    /* Запоминаем отправителя в кеш */
    arp_store(sender_ip, p->sender_mac);

    /* Если это запрос к нам — отвечаем */
    if (ntohs(p->opcode) == 1 && target_ip == NET_IP) {
        arp_packet_t reply;
        reply.htype = htons(1);
        reply.ptype = htons(ETH_TYPE_IP);
        reply.hlen  = 6;
        reply.plen  = 4;
        reply.opcode = htons(2);   /* reply */
        net_get_mac(reply.sender_mac);
        reply.sender_ip = htonl(NET_IP);
        for (int i = 0; i < 6; i++) reply.target_mac[i] = p->sender_mac[i];
        reply.target_ip = p->sender_ip;

        net_send_ethernet(p->sender_mac, ETH_TYPE_ARP, &reply, sizeof(reply));

        vga_print("ARP: reply → ");
        vga_print_dec((sender_ip >> 24) & 0xFF); vga_putchar('.');
        vga_print_dec((sender_ip >> 16) & 0xFF); vga_putchar('.');
        vga_print_dec((sender_ip >>  8) & 0xFF); vga_putchar('.');
        vga_print_dec( sender_ip        & 0xFF); vga_putchar('\n');
    }
}

/* ============================================
   Отладка
   ============================================ */
void arp_dump(void) {
    vga_print_color("=== ARP table ===\n", VGA_LIGHT_CYAN);
    if (arp_count == 0) { vga_print("(empty)\n"); return; }
    for (int i = 0; i < arp_count; i++) {
        if (!arp_cache[i].valid) continue;
        uint32_t ip = arp_cache[i].ip;
        vga_print("  ");
        vga_print_dec((ip >> 24) & 0xFF); vga_putchar('.');
        vga_print_dec((ip >> 16) & 0xFF); vga_putchar('.');
        vga_print_dec((ip >>  8) & 0xFF); vga_putchar('.');
        vga_print_dec( ip        & 0xFF);
        vga_print("  ");
        for (int j = 0; j < 6; j++) {
            char hex[] = "0123456789abcdef";
            vga_putchar(hex[arp_cache[i].mac[j] >> 4]);
            vga_putchar(hex[arp_cache[i].mac[j] & 0xF]);
            if (j < 5) vga_putchar(':');
        }
        vga_putchar('\n');
    }
}