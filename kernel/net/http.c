#include "http.h"
#include "tcp.h"
#include "dns.h"
#include "arp.h"
#include "net.h"
#include "../vga/vga.h"
#include "../kmalloc/kmalloc.h"

/* ============================================
   Парсинг URL: http://example.com/index.html
   ============================================ */
int http_parse_url(const char* url, char* host, int host_max,
                   uint16_t* port, char* path, int path_max)
{
    const char* p = url;

    if (strncmp(p, "http://", 7) == 0) p += 7;

    int hi = 0;
    while (*p && *p != '/' && *p != ':' && hi < host_max - 1) {
        host[hi++] = *p++;
    }
    host[hi] = 0;
    if (hi == 0) return 0;

    *port = 80;
    if (*p == ':') {
        p++;
        int v = 0;
        while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
        *port = (uint16_t)v;
    }

    if (*p == 0) {
        path[0] = '/';
        path[1] = 0;
    } else {
        int pi = 0;
        while (*p && pi < path_max - 1) path[pi++] = *p++;
        path[pi] = 0;
    }

    return 1;
}

/* ============================================
   Синхронный GET
   ============================================ */
char* http_get(const char* url, uint32_t* out_len) {
    *out_len = 0;

    char host[128] = {0};
    uint16_t port = 80;
    char path[512] = {0};
    if (!http_parse_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        vga_print_color("HTTP: bad URL\n", VGA_LIGHT_RED);
        return 0;
    }

    vga_print("HTTP: GET http://");
    vga_print(host);
    vga_print(path);
    vga_putchar('\n');

    /* ---------- 1. Резолвим имя ---------- */
    uint32_t dst_ip = 0;

    /* Уже IP в виде a.b.c.d? */
    int a=0, b=0, c=0, d=0, ok=1;
    const char* hp = host;
    while (*hp >= '0' && *hp <= '9') a = a*10 + (*hp++ - '0');
    if (*hp++ == '.') {
        while (*hp >= '0' && *hp <= '9') b = b*10 + (*hp++ - '0');
        if (*hp++ == '.') {
            while (*hp >= '0' && *hp <= '9') c = c*10 + (*hp++ - '0');
            if (*hp++ == '.') {
                while (*hp >= '0' && *hp <= '9') d = d*10 + (*hp++ - '0');
                if (*hp != 0) ok = 0;
            } else ok = 0;
        } else ok = 0;
    } else ok = 0;

    if (ok) {
        dst_ip = ((uint32_t)a << 24) | ((uint32_t)b << 16) |
                 ((uint32_t)c << 8)  |  (uint32_t)d;
    } else {
        /* --- ARP для DNS-сервера 10.0.2.3 --- */
        {
            uint8_t mac[6];
            if (!arp_lookup(NET_DNS, mac)) {
                arp_request(NET_DNS);
                for (int t = 0; t < 50; t++) {
                    for (volatile int k = 0; k < 5000000; k++) { }
                    net_poll();
                    if (arp_lookup(NET_DNS, mac)) break;
                }
            }
        }

        /* --- DNS --- */
        if (!dns_resolve(host, &dst_ip)) {
            vga_print_color("HTTP: DNS busy\n", VGA_LIGHT_RED);
            return 0;
        }

        for (int t = 0; t < 100; t++) {
            for (volatile int k = 0; k < 5000000; k++) { }
            net_poll();
            uint32_t r = dns_get_last();
            if (r != 0) { dst_ip = r; break; }
        }
        if (dst_ip == 0) {
            vga_print_color("HTTP: DNS timeout\n", VGA_LIGHT_RED);
            return 0;
        }
    }

    /* ---------- 2. ARP до целевого IP ---------- */
    {
        uint8_t mac[6];
        if (!arp_lookup(dst_ip, mac)) {
            arp_request(dst_ip);
            for (int t = 0; t < 50; t++) {
                for (volatile int k = 0; k < 5000000; k++) { }
                net_poll();
                if (arp_lookup(dst_ip, mac)) break;
            }
        }
    }

    /* ---------- 3. TCP connect ---------- */
    int conn = tcp_connect(dst_ip, port);
    if (conn < 0) return 0;

    for (int t = 0; t < 100; t++) {
        for (volatile int k = 0; k < 5000000; k++) { }
        net_poll();
        if (tcp_state(conn) == 2 /* ESTABLISHED */) break;
    }
    if (tcp_state(conn) != 2) {
        vga_print_color("HTTP: TCP connect failed\n", VGA_LIGHT_RED);
        tcp_close(conn);
        return 0;
    }

    /* ---------- 4. Отправляем GET ---------- */
    char req[1024];
    int ri = 0;
    const char* s1 = "GET ";
    while (*s1) req[ri++] = *s1++;
    const char* s2 = path;
    while (*s2) req[ri++] = *s2++;
    const char* s3 = " HTTP/1.0\r\nHost: ";
    while (*s3) req[ri++] = *s3++;
    const char* s4 = host;
    while (*s4) req[ri++] = *s4++;
    const char* s5 = "\r\nUser-Agent: ColibriBrowser/0.1\r\nConnection: close\r\n\r\n";
    while (*s5) req[ri++] = *s5++;

    tcp_send(conn, req, ri);

    /* ---------- 5. Читаем ответ ---------- */
    char* buf = (char*)kmalloc(HTTP_BUF_SIZE);
    if (!buf) { tcp_close(conn); return 0; }
    uint32_t total = 0;

    for (int t = 0; t < 400; t++) {
        for (volatile int k = 0; k < 3000000; k++) { }
        net_poll();

        uint32_t avail = tcp_available(conn);
        if (avail > 0) {
            uint32_t space = HTTP_BUF_SIZE - 1 - total;
            if (avail > space) avail = space;
            uint32_t got = tcp_recv(conn, buf + total, avail);
            total += got;
            if (total >= HTTP_BUF_SIZE - 1) break;
        }

        int st = tcp_state(conn);
        if (st == TCP_CLOSED || st == TCP_CLOSE_WAIT) {
            uint32_t avail2 = tcp_available(conn);
            while (avail2 > 0) {
                uint32_t space = HTTP_BUF_SIZE - 1 - total;
                if (avail2 > space) avail2 = space;
                uint32_t got = tcp_recv(conn, buf + total, avail2);
                if (got == 0) break;
                total += got;
                avail2 = tcp_available(conn);
            }
            break;
        }
    }
    buf[total] = 0;

    tcp_close(conn);

    /* ---------- 6. Отрезаем HTTP-заголовки ---------- */
    char* body = buf;
    for (uint32_t i = 0; i + 3 < total; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' &&
            buf[i+2] == '\r' && buf[i+3] == '\n') {
            body = buf + i + 4;
            break;
        }
    }

    uint32_t body_len = total - (body - buf);
    if (body != buf) {
        memmove(buf, body, body_len);
    }
    buf[body_len] = 0;
    *out_len = body_len;

    return buf;
}