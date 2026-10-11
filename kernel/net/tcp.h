#ifndef TCP_H
#define TCP_H

#include "net.h"

/* TCP флаги */
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10

/* Состояния соединения (упрощённая FSM) */
#define TCP_CLOSED      0
#define TCP_SYN_SENT    1
#define TCP_ESTABLISHED 2
#define TCP_FIN_WAIT    3
#define TCP_CLOSE_WAIT  4
#define TCP_TIME_WAIT   5

/* Максимум одновременно открытых соединений */
#define TCP_MAX_CONN    4

/* Размер приёмного буфера одного соединения */
#define TCP_RX_BUF_SIZE 16384

typedef struct __attribute__((packed)) {
    uint16_t src_port;      /* big-endian */
    uint16_t dst_port;      /* big-endian */
    uint32_t seq;           /* big-endian */
    uint32_t ack;           /* big-endian */
    uint8_t  data_offset;   /* верхние 4 бита — размер заголовка /4 */
    uint8_t  flags;
    uint16_t window;        /* big-endian */
    uint16_t checksum;
    uint16_t urgent;
} tcp_header_t;

/* Псевдо-заголовок для checksum */
typedef struct __attribute__((packed)) {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t tcp_len;
} tcp_pseudo_t;

typedef struct {
    int      state;
    uint32_t remote_ip;     /* host-order */
    uint16_t remote_port;   /* host-order */
    uint16_t local_port;    /* host-order */

    uint32_t snd_nxt;       /* следующий seq для отправки */
    uint32_t snd_una;       /* подтверждено (unacknowledged = первое неподтверждённое) */
    uint32_t rcv_nxt;       /* следующий ожидаемый seq от собеседника */

    /* Приёмный буфер для данных */
    uint8_t  rx_buf[TCP_RX_BUF_SIZE];
    uint32_t rx_len;        /* сколько данных накопилось */

    int      got_fin;       /* получили FIN от собеседника */
} tcp_conn_t;

void tcp_init(void);
void tcp_handle(const uint8_t* payload, uint32_t len);

/* Открыть соединение: возвращает индекс (0..TCP_MAX_CONN-1) или -1 */
int  tcp_connect(uint32_t dst_ip, uint16_t dst_port);

/* Состояние соединения */
int  tcp_state(int conn);

/* Отправить данные */
int  tcp_send(int conn, const void* data, uint32_t len);

/* Закрыть (послать FIN) */
void tcp_close(int conn);

/* Есть ли данные для чтения. Возвращает кол-во байт. */
uint32_t tcp_available(int conn);

/* Прочитать до len байт. Возвращает сколько реально прочитано. */
uint32_t tcp_recv(int conn, void* buf, uint32_t len);

/* Опрос всех соединений — вызывается из главного цикла */
void tcp_poll(void);

#endif