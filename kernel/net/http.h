#ifndef HTTP_H
#define HTTP_H

#include "net.h"

#define HTTP_BUF_SIZE 65536

/* Простой синхронный GET-запрос.
   Возвращает: указатель на тело ответа (heap), длину в *out_len, 0 при ошибке. */
char* http_get(const char* url, uint32_t* out_len);

/* Только распарсить URL: http://host[:port]/path */
int http_parse_url(const char* url, char* host, int host_max,
                   uint16_t* port, char* path, int path_max);

#endif