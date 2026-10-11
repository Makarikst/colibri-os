#include "tcp.h"
#include "ip.h"
#include "net.h"
#include "../vga/vga.h"

static tcp_conn_t conns[TCP_MAX_CONN];
static uint16_t   next_local_port = 40000;

/* ============================================
   Init
   ============================================ */
void tcp_init(void) {
    for (int i = 0; i < TCP_MAX_CONN; i++) {
        conns[i].state = TCP_CLOSED;
        conns[i].rx_len = 0;
        conns[i].got_fin = 0;
    }
}

/* ============================================
   Checksum (псевдо + TCP заголовок + данные)
   ============================================ */
static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                              const void* tcp_seg, uint32_t tcp_len)
{
    static uint8_t buf[12 + 1500];
    tcp_pseudo_t* ph = (tcp_pseudo_t*)buf;
    ph->src_ip  = htonl(src_ip);
    ph->dst_ip  = htonl(dst_ip);
    ph->zero    = 0;
    ph->proto   = IP_PROTO_TCP;
    ph->tcp_len = htons(tcp_len);

    if (tcp_len > 1500) tcp_len = 1500;
    memcpy(buf + 12, tcp_seg, tcp_len);

    return ip_checksum(buf, 12 + tcp_len);
}

/* ============================================
   Отправка сегмента
   ============================================ */
static int tcp_send_segment(tcp_conn_t* c, uint8_t flags,
                            const void* data, uint32_t len)
{
    static uint8_t seg[20 + 1460];
    tcp_header_t* h = (tcp_header_t*)seg;

    h->src_port    = htons(c->local_port);
    h->dst_port    = htons(c->remote_port);
    h->seq         = htonl(c->snd_nxt);
    h->ack         = htonl(c->rcv_nxt);
    h->data_offset = (20 / 4) << 4;
    h->flags       = flags;
    h->window      = htons(TCP_RX_BUF_SIZE);
    h->checksum    = 0;
    h->urgent      = 0;

    if (len > 0) memcpy(seg + 20, data, len);
    h->checksum = tcp_checksum(NET_IP, c->remote_ip, seg, 20 + len);

    int ok = ip_send(c->remote_ip, IP_PROTO_TCP, seg, 20 + len);

    /* Продвигаем seq, если это не чистый ACK */
    if (flags & (TCP_SYN | TCP_FIN)) c->snd_nxt += 1;
    c->snd_nxt += len;

    return ok;
}

/* ============================================
   Открытие соединения
   ============================================ */
int tcp_connect(uint32_t dst_ip, uint16_t dst_port) {
    int idx = -1;
    for (int i = 0; i < TCP_MAX_CONN; i++) {
        if (conns[i].state == TCP_CLOSED) { idx = i; break; }
    }
    if (idx < 0) {
        vga_print_color("TCP: no free slots\n", VGA_LIGHT_RED);
        return -1;
    }

    tcp_conn_t* c = &conns[idx];
    c->remote_ip    = dst_ip;
    c->remote_port  = dst_port;
    c->local_port   = next_local_port++;
    c->snd_nxt      = (uint32_t)(rand_u32() | 1);
    c->snd_una      = c->snd_nxt;
    c->rcv_nxt      = 0;
    c->rx_len       = 0;
    c->got_fin      = 0;

    c->state = TCP_SYN_SENT;

    /* Отладка */
    vga_print("[TCP] connect to ");
    vga_print_dec((dst_ip >> 24) & 0xFF); vga_putchar('.');
    vga_print_dec((dst_ip >> 16) & 0xFF); vga_putchar('.');
    vga_print_dec((dst_ip >>  8) & 0xFF); vga_putchar('.');
    vga_print_dec( dst_ip        & 0xFF);
    vga_print(":");
    vga_print_dec(dst_port);
    vga_print(" local_port=");
    vga_print_dec(c->local_port);
    vga_print(" seq=");
    vga_print_dec(c->snd_nxt);
    vga_putchar('\n');

    /* === СНАЧАЛА разрешаем ARP для next hop === */
    {
        extern int arp_lookup(uint32_t, uint8_t*);
        extern int arp_request(uint32_t);
        extern void net_poll(void);

        uint32_t next_hop = dst_ip;
        if ((dst_ip & NET_MASK) != (NET_IP & NET_MASK)) {
            next_hop = NET_GW;
        }
        uint8_t mac[6];
        if (!arp_lookup(next_hop, mac)) {
            vga_print_color("[TCP] ARP MISS, requesting...\n", VGA_YELLOW);
            arp_request(next_hop);
            for (int t = 0; t < 100; t++) {
                for (volatile int k = 0; k < 5000000; k++) { }
                net_poll();
                if (arp_lookup(next_hop, mac)) {
                    vga_print("[TCP] ARP resolved!\n");
                    break;
                }
            }
            if (!arp_lookup(next_hop, mac)) {
                vga_print_color("[TCP] ARP timeout!\n", VGA_LIGHT_RED);
                return idx;   /* соединение не установится, но вернём */
            }
        } else {
            vga_print("[TCP] ARP OK\n");
        }
    }

    /* === А ТЕПЕРЬ шлём SYN === */
    tcp_send_segment(c, TCP_SYN, 0, 0);

    return idx;

    return idx;
}

/* ============================================
   Обработка входящего TCP
   ============================================ */
void tcp_handle(const uint8_t* payload, uint32_t len) {
    if (len < 20) return;

    /* Достаём src/dst IP из IP-заголовка — упрощение: мы знаем, что это не мы */
    /* Но у нас ip_handle не передаёт src_ip. Найдём соединение по портам. */
    const tcp_header_t* h = (const tcp_header_t*)payload;
    uint16_t src_port = ntohs(h->src_port);
    uint16_t dst_port = ntohs(h->dst_port);
    uint32_t seq      = ntohl(h->seq);
    uint32_t ack      = ntohl(h->ack);
    uint8_t  flags    = h->flags;
    uint32_t hdr_len  = (h->data_offset >> 4) * 4;
    if (hdr_len < 20 || hdr_len > len) return;
    uint32_t data_len = len - hdr_len;

    /* Ищем соединение по local_port == dst_port */
    vga_print("[TCP] RX: src=");
    vga_print_dec(src_port);
    vga_print(" dst=");
    vga_print_dec(dst_port);
    vga_print(" flags=0x");
    vga_print_hex32(flags);
    vga_print(" seq=");
    vga_print_dec(seq);
    vga_print(" ack=");
    vga_print_dec(ack);
    vga_print(" len=");
    vga_print_dec(data_len);
    vga_putchar('\n');

    tcp_conn_t* c = 0;
    for (int i = 0; i < TCP_MAX_CONN; i++) {
        if (conns[i].state != TCP_CLOSED &&
            conns[i].local_port == dst_port &&
            conns[i].remote_port == src_port)
        {
            c = &conns[i];
            break;
        }
    }
    if (!c) {
        vga_print_color("[TCP] no conn for this segment!\n", VGA_LIGHT_RED);
        return;
    }

    /* ============================
       SYN_SENT — ждём SYN/ACK
       ============================ */
    if (c->state == TCP_SYN_SENT) {
        if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            c->rcv_nxt = seq + 1;
            c->snd_una = ack;
            c->state   = TCP_ESTABLISHED;

            /* Отправляем ACK */
            tcp_send_segment(c, TCP_ACK, 0, 0);
        }
        return;
    }

    /* ============================
       ESTABLISHED / FIN_WAIT — принимаем данные
       ============================ */
    if (c->state == TCP_ESTABLISHED || c->state == TCP_FIN_WAIT) {
        /* Подтверждаем всё, что пришло */
        if (data_len > 0 && seq == c->rcv_nxt) {
            uint32_t space = TCP_RX_BUF_SIZE - c->rx_len;
            uint32_t to_copy = data_len;
            if (to_copy > space) to_copy = space;

            memcpy(c->rx_buf + c->rx_len, payload + hdr_len, to_copy);
            c->rx_len += to_copy;
            c->rcv_nxt = seq + to_copy;

            /* ACK на полученные данные */
            tcp_send_segment(c, TCP_ACK, 0, 0);
        }

        if (flags & TCP_FIN) {
            c->got_fin = 1;
            c->rcv_nxt = seq + (data_len > 0 ? data_len : 0) + 1;
            tcp_send_segment(c, TCP_ACK, 0, 0);
            c->state = TCP_CLOSE_WAIT;
        }
    }
}

/* ============================================
   Отправка данных
   ============================================ */
int tcp_send(int conn, const void* data, uint32_t len) {
    if (conn < 0 || conn >= TCP_MAX_CONN) return 0;
    tcp_conn_t* c = &conns[conn];
    if (c->state != TCP_ESTABLISHED) return 0;

    const uint8_t* p = (const uint8_t*)data;
    uint32_t remaining = len;

    while (remaining > 0) {
        uint32_t chunk = remaining > 1460 ? 1460 : remaining;
        tcp_send_segment(c, TCP_ACK | TCP_PSH, p, chunk);
        p += chunk;
        remaining -= chunk;
    }
    return len;
}

/* ============================================
   Закрытие
   ============================================ */
void tcp_close(int conn) {
    if (conn < 0 || conn >= TCP_MAX_CONN) return;
    tcp_conn_t* c = &conns[conn];
    if (c->state == TCP_ESTABLISHED) {
        tcp_send_segment(c, TCP_FIN | TCP_ACK, 0, 0);
        c->state = TCP_FIN_WAIT;
    }
}

/* ============================================
   Чтение
   ============================================ */
uint32_t tcp_available(int conn) {
    if (conn < 0 || conn >= TCP_MAX_CONN) return 0;
    return conns[conn].rx_len;
}

uint32_t tcp_recv(int conn, void* buf, uint32_t len) {
    if (conn < 0 || conn >= TCP_MAX_CONN) return 0;
    tcp_conn_t* c = &conns[conn];
    uint32_t n = c->rx_len < len ? c->rx_len : len;
    if (n > 0) {
        memcpy(buf, c->rx_buf, n);
        for (uint32_t i = n; i < c->rx_len; i++)
            c->rx_buf[i - n] = c->rx_buf[i];
        c->rx_len -= n;
    }
    /* Если FIN получен и буфер пуст — закрываем */
    if (c->got_fin && c->rx_len == 0 && c->state == TCP_CLOSE_WAIT) {
        c->state = TCP_CLOSED;
    }
    return n;
}

int tcp_state(int conn) {
    if (conn < 0 || conn >= TCP_MAX_CONN) return TCP_CLOSED;
    return conns[conn].state;
}

/* ============================================
   Poll
   ============================================ */
void tcp_poll(void) {
    /* Пока пусто — всё делается в tcp_handle */
}