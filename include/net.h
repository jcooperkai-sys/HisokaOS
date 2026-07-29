/* net.h - minimal networking: IP config + ARP resolution over the RTL8139. */
#ifndef HISOKA_NET_H
#define HISOKA_NET_H
#include "types.h"

const uint8_t *net_my_ip(void);                         /* our static IPv4 (10.0.2.15) */
const uint8_t *net_gw_ip(void);                         /* gateway (10.0.2.2)          */
int  net_parse_ip(const char *s, uint8_t *ip);          /* "a.b.c.d" -> 4 bytes, 1 ok  */
int  net_arp_resolve(const uint8_t *ip, uint8_t *mac);  /* 1 = got reply, 0 = timeout  */
int  net_ping(const uint8_t *ip, int *ttl);             /* ICMP echo: 1 = reply, 0 = timeout */
int  net_dns_resolve(const char *host, uint8_t *ip);    /* UDP/DNS A lookup: 1 = ok */
int  net_http_get(const char *host, const char *path, char *out, int maxout);  /* TCP/HTTP GET (:80) */
int  net_http_get_ep(const char *host, uint16_t port, const char *path, char *out, int maxout); /* IP/host + port */

#endif
