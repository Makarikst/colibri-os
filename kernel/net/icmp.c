#include "icmp.h"
#include "ip.h"
#include "../vga/vga.h"

void icmp_init(void) { }

int icmp_ping(uint32_t dst_ip, uint16_t id, uint16_t seq) {
    static uint8_t buf[8 + 32];
    icmp_header_t* icmp = (icmp_header_t*)buf;
    icmp->type = 8;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);

    for (int i = 0; i < 32; i++) buf[8 + i] = (uint8_t)(i + 'A');

    icmp->checksum = ip_checksum(buf, 8 + 32);

    return ip_send(dst_ip, IP_PROTO_ICMP, buf, 8 + 32);
}

void icmp_handle(const uint8_t* payload, uint32_t len) {
    if (len < 8) return;

    const icmp_header_t* icmp = (const icmp_header_t*)payload;

    if (icmp->type == 0) {
        vga_print_color("PING reply: id=", VGA_LIGHT_GREEN);
        vga_print_dec(ntohs(icmp->id));
        vga_print(" seq=");
        vga_print_dec(ntohs(icmp->seq));
        vga_print(" bytes=");
        vga_print_dec(len);
        vga_putchar('\n');
    }
}