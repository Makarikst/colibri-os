#ifndef DNS_H
#define DNS_H

#include "net.h"

void dns_init(void);
void dns_handle(const uint8_t* payload, uint32_t len);

int  dns_resolve(const char* hostname, uint32_t* res_ip);

void dns_poll(void);
void dns_dump(void);

/* Результат последнего разрешения (0, если ещё нет) */
uint32_t dns_get_last(void);

#endif