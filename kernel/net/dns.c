#include "dns.h"
#include "udp.h"
#include "../vga/vga.h"

/* Один ожидающий запрос */
static char     pending_host[128];
static uint32_t pending_result = 0;
static int      pending_active = 0;
static uint16_t dns_txid       = 0x1234;
static uint16_t dns_local_port = 53000;

void dns_init(void) {
    pending_active = 0;
    pending_result = 0;
    pending_host[0] = 0;
}

/* ============================================
   Кодирование имени в DNS-формат
   ============================================ */
static int dns_encode_name(const char* host, uint8_t* out, int max) {
    int pos = 0;
    const char* p = host;
    while (*p) {
        const char* dot = p;
        while (*dot && *dot != '.') dot++;
        int seg = dot - p;
        if (seg == 0 || seg > 63) return -1;
        if (pos + 1 + seg + 1 > max) return -1;
        out[pos++] = (uint8_t)seg;
        for (int i = 0; i < seg; i++) out[pos++] = (uint8_t)p[i];
        p = dot;
        if (*p == '.') p++;
    }
    out[pos++] = 0;
    return pos;
}

/* ============================================
   Отправка DNS-запроса
   ============================================ */
int dns_resolve(const char* hostname, uint32_t* res_ip) {
    if (pending_active) return 0;

    static uint8_t packet[512];
    memset(packet, 0, 12);

    packet[0] = (dns_txid >> 8) & 0xFF;
    packet[1] = dns_txid & 0xFF;
    packet[2] = 0x01;
    packet[3] = 0x00;
    packet[4] = 0; packet[5] = 1;
    packet[6] = 0; packet[7] = 0;
    packet[8] = 0; packet[9] = 0;
    packet[10]= 0; packet[11]= 0;

    int pos = 12;
    int nlen = dns_encode_name(hostname, packet + pos, 256);
    if (nlen < 0) return 0;
    pos += nlen;

    packet[pos++] = 0;
    packet[pos++] = 1;
    packet[pos++] = 0;
    packet[pos++] = 1;

    int i = 0;
    while (hostname[i] && i < 127) { pending_host[i] = hostname[i]; i++; }
    pending_host[i] = 0;
    pending_active = 1;
    pending_result = 0;

    udp_send(NET_DNS, dns_local_port, 53, packet, pos);

    return 1;
}

/* ============================================
   Обработка ответа
   ============================================ */
void dns_handle(const uint8_t* payload, uint32_t len) {
    if (!pending_active) return;
    if (len < 12) return;

    uint16_t txid = (payload[0] << 8) | payload[1];
    if (txid != dns_txid) return;

    uint16_t flags   = (payload[2] << 8) | payload[3];
    uint16_t ancount = (payload[6] << 8) | payload[7];
    if ((flags & 0x8000) == 0) return;
    if (ancount == 0) return;

    int pos = 12;
    uint16_t qdcount = (payload[4] << 8) | payload[5];
    for (int q = 0; q < qdcount; q++) {
        while (pos < (int)len) {
            uint8_t l = payload[pos];
            if (l == 0) { pos++; break; }
            if ((l & 0xC0) == 0xC0) { pos += 2; break; }
            pos += 1 + l;
        }
        pos += 4;
    }

    for (int a = 0; a < ancount && pos < (int)len; a++) {
        while (pos < (int)len) {
            uint8_t l = payload[pos];
            if (l == 0) { pos++; break; }
            if ((l & 0xC0) == 0xC0) { pos += 2; break; }
            pos += 1 + l;
        }
        if (pos + 10 > (int)len) break;
        uint16_t type   = (payload[pos] << 8) | payload[pos+1];
        uint16_t rdlen  = (payload[pos+8] << 8) | payload[pos+9];
        pos += 10;

        if (type == 1 && rdlen == 4) {
            uint32_t ip = ((uint32_t)payload[pos]     << 24) |
                          ((uint32_t)payload[pos + 1] << 16) |
                          ((uint32_t)payload[pos + 2] <<  8) |
                           (uint32_t)payload[pos + 3];
            pending_result = ip;
            vga_print_color("DNS: ", VGA_LIGHT_GREEN);
            vga_print(pending_host);
            vga_print(" -> ");
            vga_print_dec((ip >> 24) & 0xFF); vga_putchar('.');
            vga_print_dec((ip >> 16) & 0xFF); vga_putchar('.');
            vga_print_dec((ip >>  8) & 0xFF); vga_putchar('.');
            vga_print_dec( ip        & 0xFF);
            vga_putchar('\n');
            pending_active = 0;
            return;
        }
        pos += rdlen;
    }
    pending_active = 0;
}

void dns_poll(void) { }

void dns_dump(void) {
    vga_print("DNS pending: ");
    vga_print(pending_active ? "yes" : "no");
    vga_print(", host=");
    vga_print(pending_host);
    vga_print(", result=");
    vga_print_hex32(pending_result);
    vga_putchar('\n');
}

uint32_t dns_get_last(void) { return pending_result; }