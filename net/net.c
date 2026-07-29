/* net.c - the first real networking on HisokaOS: ARP over Ethernet.
 *
 * We give the guest a fixed address on QEMU's user-mode network (10.0.2.15, gateway
 * 10.0.2.2). net_arp_resolve() broadcasts an ARP request for an IP and polls the
 * NIC for the reply, returning the hardware address that answered. That a frame we
 * built goes out and a reply comes back proves the whole TX/RX path works end to
 * end - the foundation every higher protocol (IP, ICMP/ping, UDP, TCP) builds on. */
#include "net.h"
#include "rtl8139.h"
#include "pit.h"

/* wall-clock poll deadlines (PIT runs at 100 Hz, so 1 tick = 10 ms) */
#define ARP_TICKS  50    /* 0.5 s per ARP attempt */
#define NET_TICKS  1800  /* 18 s for a reply (host Chromium render can take seconds) */

static const uint8_t my_ip[4] = { 10, 0, 2, 15 };
static const uint8_t gw_ip[4] = { 10, 0, 2, 2 };

const uint8_t *net_my_ip(void) { return my_ip; }
const uint8_t *net_gw_ip(void) { return gw_ip; }

int net_parse_ip(const char *s, uint8_t *ip) {
    int part = 0, val = 0, digits = 0;
    for (;; s++) {
        if (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); digits++; if (val > 255) return 0; }
        else if (*s == '.' || *s == 0) {
            if (!digits || part > 3) return 0;
            ip[part++] = (uint8_t)val; val = 0; digits = 0;
            if (*s == 0) break;
        } else return 0;
    }
    return part == 4;
}

static void build_arp_request(uint8_t *f, const uint8_t *tip) {
    const uint8_t *m = rtl8139_mac();
    for (int i = 0; i < 6; i++) f[i] = 0xFF;          /* dst: broadcast */
    for (int i = 0; i < 6; i++) f[6 + i] = m[i];      /* src: our MAC   */
    f[12] = 0x08; f[13] = 0x06;                       /* ethertype ARP  */
    uint8_t *a = f + 14;
    a[0] = 0x00; a[1] = 0x01;                         /* HTYPE ethernet */
    a[2] = 0x08; a[3] = 0x00;                         /* PTYPE IPv4     */
    a[4] = 6;    a[5] = 4;                            /* hlen, plen     */
    a[6] = 0x00; a[7] = 0x01;                         /* OPER request   */
    for (int i = 0; i < 6; i++) a[8 + i]  = m[i];     /* sender MAC     */
    for (int i = 0; i < 4; i++) a[14 + i] = my_ip[i]; /* sender IP      */
    for (int i = 0; i < 6; i++) a[18 + i] = 0;        /* target MAC ?   */
    for (int i = 0; i < 4; i++) a[24 + i] = tip[i];   /* target IP      */
}

int net_arp_resolve(const uint8_t *tip, uint8_t *mac_out) {
    uint8_t frame[42];
    build_arp_request(frame, tip);
    uint8_t buf[1600];
    for (int attempt = 0; attempt < 4; attempt++) {        /* resend if no reply */
        rtl8139_flush();
        rtl8139_send(frame, 42);
        uint32_t deadline = pit_ticks() + ARP_TICKS;
        while (pit_ticks() < deadline) {
            uint16_t len = rtl8139_recv(buf, sizeof(buf));
            if (len >= 42 && buf[12] == 0x08 && buf[13] == 0x06) {    /* ARP frame */
                uint8_t *a = buf + 14;
                uint16_t oper = (uint16_t)((a[6] << 8) | a[7]);
                if (oper == 2 &&                                      /* reply */
                    a[14] == tip[0] && a[15] == tip[1] &&
                    a[16] == tip[2] && a[17] == tip[3]) {
                    for (int i = 0; i < 6; i++) mac_out[i] = a[8 + i];
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* 16-bit one's-complement checksum (IP / ICMP), over big-endian header bytes */
static uint16_t checksum(const uint8_t *d, int len) {
    uint32_t sum = 0;
    for (int i = 0; i + 1 < len; i += 2) sum += (uint32_t)((d[i] << 8) | d[i+1]);
    if (len & 1) sum += (uint32_t)(d[len-1] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint16_t ip_id = 0x4869;

/* send an ICMP echo request to ip and wait for the echo reply */
int net_ping(const uint8_t *target, int *ttl_out) {
    /* next hop: the target itself if on our /24, otherwise the gateway */
    uint8_t nh[4];
    if (target[0] == my_ip[0] && target[1] == my_ip[1] && target[2] == my_ip[2])
        for (int i = 0; i < 4; i++) nh[i] = target[i];
    else
        for (int i = 0; i < 4; i++) nh[i] = gw_ip[i];

    uint8_t dmac[6];
    if (!net_arp_resolve(nh, dmac)) return 0;

    uint8_t f[64];
    const uint8_t *m = rtl8139_mac();
    for (int i = 0; i < 6; i++) f[i] = dmac[i];           /* eth dst */
    for (int i = 0; i < 6; i++) f[6 + i] = m[i];          /* eth src */
    f[12] = 0x08; f[13] = 0x00;                           /* IPv4    */

    uint8_t *ip = f + 14;
    int iplen = 20 + 8;                                   /* IP hdr + ICMP (no payload) */
    ip[0] = 0x45; ip[1] = 0x00;
    ip[2] = (uint8_t)(iplen >> 8); ip[3] = (uint8_t)(iplen & 0xFF);
    ip[4] = (uint8_t)(ip_id >> 8); ip[5] = (uint8_t)(ip_id & 0xFF); ip_id++;
    ip[6] = 0x00; ip[7] = 0x00;                           /* flags/frag */
    ip[8] = 64;   ip[9] = 1;                              /* ttl, proto ICMP */
    ip[10] = 0; ip[11] = 0;
    for (int i = 0; i < 4; i++) ip[12 + i] = my_ip[i];
    for (int i = 0; i < 4; i++) ip[16 + i] = target[i];
    uint16_t ic = checksum(ip, 20);
    ip[10] = (uint8_t)(ic >> 8); ip[11] = (uint8_t)(ic & 0xFF);

    uint8_t *icmp = f + 14 + 20;
    icmp[0] = 8; icmp[1] = 0;                             /* echo request */
    icmp[2] = 0; icmp[3] = 0;
    icmp[4] = 0x00; icmp[5] = 0x01;                       /* id  */
    icmp[6] = 0x00; icmp[7] = 0x01;                       /* seq */
    uint16_t cc = checksum(icmp, 8);
    icmp[2] = (uint8_t)(cc >> 8); icmp[3] = (uint8_t)(cc & 0xFF);

    rtl8139_send(f, (uint16_t)(14 + iplen));

    uint8_t buf[1600];
    uint32_t deadline = pit_ticks() + NET_TICKS;
    while (pit_ticks() < deadline) {
        uint16_t len = rtl8139_recv(buf, sizeof(buf));
        if (len >= 14 + 20 + 8 && buf[12] == 0x08 && buf[13] == 0x00) {  /* IPv4 */
            uint8_t *rip = buf + 14;
            if ((rip[0] >> 4) == 4 && rip[9] == 1) {                     /* ICMP */
                int ihl = (rip[0] & 0x0F) * 4;
                uint8_t *ricmp = buf + 14 + ihl;
                if (ricmp[0] == 0 &&                                     /* echo reply */
                    rip[12] == target[0] && rip[13] == target[1] &&
                    rip[14] == target[2] && rip[15] == target[3]) {
                    if (ttl_out) *ttl_out = rip[8];
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* UDP checksum, including the IPv4 pseudo-header */
static uint16_t udp_checksum(const uint8_t *sip, const uint8_t *dip, const uint8_t *udp, int udplen) {
    uint8_t tmp[1600]; int n = 0;
    for (int i = 0; i < 4; i++) tmp[n++] = sip[i];
    for (int i = 0; i < 4; i++) tmp[n++] = dip[i];
    tmp[n++] = 0; tmp[n++] = 17;
    tmp[n++] = (uint8_t)(udplen >> 8); tmp[n++] = (uint8_t)(udplen & 0xFF);
    for (int i = 0; i < udplen; i++) tmp[n++] = udp[i];
    return checksum(tmp, n);
}

/* resolve a hostname to an IPv4 address via the QEMU DNS server (10.0.2.3) */
int net_dns_resolve(const char *host, uint8_t *ip_out) {
    static const uint8_t dns_ip[4] = { 10, 0, 2, 3 };
    uint8_t dmac[6];
    if (!net_arp_resolve(dns_ip, dmac)) return 0;

    /* DNS query: header + question */
    uint8_t dns[300]; int dn = 0;
    dns[dn++] = 0x12; dns[dn++] = 0x34;        /* id */
    dns[dn++] = 0x01; dns[dn++] = 0x00;        /* flags: recursion desired */
    dns[dn++] = 0; dns[dn++] = 1;              /* QDCOUNT = 1 */
    dns[dn++] = 0; dns[dn++] = 0;              /* ANCOUNT */
    dns[dn++] = 0; dns[dn++] = 0;              /* NSCOUNT */
    dns[dn++] = 0; dns[dn++] = 0;              /* ARCOUNT */
    const char *p = host;
    while (*p) {                                /* QNAME: length-prefixed labels */
        const char *st = p; int l = 0;
        while (*p && *p != '.') { p++; l++; }
        if (l > 63) return 0;
        dns[dn++] = (uint8_t)l;
        for (int i = 0; i < l; i++) dns[dn++] = (uint8_t)st[i];
        if (*p == '.') p++;
    }
    dns[dn++] = 0;
    dns[dn++] = 0; dns[dn++] = 1;              /* QTYPE  = A  */
    dns[dn++] = 0; dns[dn++] = 1;              /* QCLASS = IN */

    /* wrap in Ethernet + IPv4 + UDP */
    uint8_t f[400];
    const uint8_t *m = rtl8139_mac();
    for (int i = 0; i < 6; i++) f[i] = dmac[i];
    for (int i = 0; i < 6; i++) f[6 + i] = m[i];
    f[12] = 0x08; f[13] = 0x00;
    uint8_t *ip = f + 14;
    int udplen = 8 + dn, iptot = 20 + udplen;
    ip[0] = 0x45; ip[1] = 0; ip[2] = (uint8_t)(iptot >> 8); ip[3] = (uint8_t)(iptot & 0xFF);
    ip[4] = 0x13; ip[5] = 0x38; ip[6] = 0; ip[7] = 0; ip[8] = 64; ip[9] = 17; ip[10] = 0; ip[11] = 0;
    for (int i = 0; i < 4; i++) ip[12 + i] = my_ip[i];
    for (int i = 0; i < 4; i++) ip[16 + i] = dns_ip[i];
    uint16_t ipc = checksum(ip, 20); ip[10] = (uint8_t)(ipc >> 8); ip[11] = (uint8_t)(ipc & 0xFF);

    uint8_t *udp = f + 14 + 20;
    udp[0] = 0xC3; udp[1] = 0x50;              /* src port 50000 */
    udp[2] = 0x00; udp[3] = 53;                /* dst port 53    */
    udp[4] = (uint8_t)(udplen >> 8); udp[5] = (uint8_t)(udplen & 0xFF);
    udp[6] = 0; udp[7] = 0;
    for (int i = 0; i < dn; i++) udp[8 + i] = dns[i];
    uint16_t uc = udp_checksum(my_ip, dns_ip, udp, udplen);
    if (uc == 0) uc = 0xFFFF;
    udp[6] = (uint8_t)(uc >> 8); udp[7] = (uint8_t)(uc & 0xFF);

    rtl8139_send(f, (uint16_t)(14 + iptot));

    /* read the response and pull out the first A record */
    uint8_t buf[1600];
    uint32_t deadline = pit_ticks() + NET_TICKS;
    while (pit_ticks() < deadline) {
        uint16_t len = rtl8139_recv(buf, sizeof(buf));
        if (len >= 14 + 20 + 8 && buf[12] == 0x08 && buf[13] == 0x00) {
            uint8_t *rip = buf + 14;
            if (rip[9] == 17) {                                  /* UDP */
                int ihl = (rip[0] & 0x0F) * 4;
                uint8_t *u = buf + 14 + ihl;
                if (((u[0] << 8) | u[1]) == 53) {                /* from port 53 */
                    uint8_t *d = u + 8;
                    int dmax = (int)len - (14 + ihl + 8);        /* valid bytes in d[] */
                    if (dmax < 12) return 0;                     /* not even a DNS header */
                    int ancount = (d[6] << 8) | d[7];
                    int pos = 12;
                    while (pos < dmax && d[pos] != 0) pos += d[pos] + 1;   /* skip question name */
                    pos += 1 + 4;                                /* null + qtype + qclass */
                    for (int a = 0; a < ancount && pos + 10 <= dmax; a++) {
                        if ((d[pos] & 0xC0) == 0xC0) pos += 2;   /* compressed name */
                        else { while (pos < dmax && d[pos] != 0) pos += d[pos] + 1; pos++; }
                        if (pos + 10 > dmax) break;              /* truncated record */
                        int type  = (d[pos] << 8) | d[pos + 1];
                        int rdlen = (d[pos + 8] << 8) | d[pos + 9];
                        int rdata = pos + 10;
                        if (type == 1 && rdlen == 4 && rdata + 4 <= dmax) {   /* A record */
                            for (int i = 0; i < 4; i++) ip_out[i] = d[rdata + i];
                            return 1;
                        }
                        pos = rdata + rdlen;
                    }
                    return 0;
                }
            }
        }
    }
    return 0;
}

/* ---- minimal TCP, just enough for one HTTP GET ----
 * QEMU's user-mode NAT delivers segments reliably and in order, so we can skip
 * retransmission/reordering and run a straight-line handshake -> request -> read. */
static uint16_t tcp_checksum(const uint8_t *sip, const uint8_t *dip, const uint8_t *tcp, int tlen) {
    uint8_t tmp[2048]; int n = 0;
    for (int i = 0; i < 4; i++) tmp[n++] = sip[i];
    for (int i = 0; i < 4; i++) tmp[n++] = dip[i];
    tmp[n++] = 0; tmp[n++] = 6;
    tmp[n++] = (uint8_t)(tlen >> 8); tmp[n++] = (uint8_t)(tlen & 0xFF);
    for (int i = 0; i < tlen && n < (int)sizeof(tmp); i++) tmp[n++] = tcp[i];
    return checksum(tmp, n);
}

static void tcp_send(const uint8_t *dmac, const uint8_t *dip, uint16_t sport, uint16_t dport,
                     uint32_t seq, uint32_t ack, uint8_t flags, const uint8_t *data, int dlen) {
    uint8_t f[1600]; const uint8_t *m = rtl8139_mac();
    for (int i = 0; i < 6; i++) f[i] = dmac[i];
    for (int i = 0; i < 6; i++) f[6 + i] = m[i];
    f[12] = 0x08; f[13] = 0x00;
    uint8_t *ip = f + 14;
    int tlen = 20 + dlen, iptot = 20 + tlen;
    ip[0] = 0x45; ip[1] = 0; ip[2] = (uint8_t)(iptot >> 8); ip[3] = (uint8_t)(iptot & 0xFF);
    ip[4] = 0; ip[5] = 0; ip[6] = 0x40; ip[7] = 0;            /* don't fragment */
    ip[8] = 64; ip[9] = 6; ip[10] = 0; ip[11] = 0;
    for (int i = 0; i < 4; i++) ip[12 + i] = my_ip[i];
    for (int i = 0; i < 4; i++) ip[16 + i] = dip[i];
    uint16_t ipc = checksum(ip, 20); ip[10] = (uint8_t)(ipc >> 8); ip[11] = (uint8_t)(ipc & 0xFF);
    uint8_t *t = ip + 20;
    t[0]=(uint8_t)(sport>>8); t[1]=(uint8_t)sport; t[2]=(uint8_t)(dport>>8); t[3]=(uint8_t)dport;
    t[4]=(uint8_t)(seq>>24); t[5]=(uint8_t)(seq>>16); t[6]=(uint8_t)(seq>>8); t[7]=(uint8_t)seq;
    t[8]=(uint8_t)(ack>>24); t[9]=(uint8_t)(ack>>16); t[10]=(uint8_t)(ack>>8); t[11]=(uint8_t)ack;
    t[12]=0x50; t[13]=flags; t[14]=0xFF; t[15]=0xFF; t[16]=0; t[17]=0; t[18]=0; t[19]=0;
    for (int i = 0; i < dlen; i++) t[20 + i] = data[i];
    uint16_t tc = tcp_checksum(my_ip, dip, t, tlen); t[16]=(uint8_t)(tc>>8); t[17]=(uint8_t)(tc&0xFF);
    rtl8139_send(f, (uint16_t)(14 + iptot));
}

/* poll for a TCP segment belonging to our connection */
static int tcp_recv(uint16_t sport, uint16_t dport, uint32_t *rseq, uint32_t *rack,
                    uint8_t *rflags, uint8_t *payload, int *plen, int maxp) {
    uint8_t buf[1600];
    uint32_t deadline = pit_ticks() + NET_TICKS;
    while (pit_ticks() < deadline) {
        uint16_t len = rtl8139_recv(buf, sizeof(buf));
        if (len >= 54 && buf[12] == 0x08 && buf[13] == 0x00) {
            uint8_t *ip = buf + 14;
            if (ip[9] == 6) {
                int ihl = (ip[0] & 0x0F) * 4;
                uint8_t *t = buf + 14 + ihl;
                uint16_t ts = (uint16_t)((t[0] << 8) | t[1]), td = (uint16_t)((t[2] << 8) | t[3]);
                if (ts == dport && td == sport) {
                    *rseq = ((uint32_t)t[4]<<24)|((uint32_t)t[5]<<16)|((uint32_t)t[6]<<8)|t[7];
                    *rack = ((uint32_t)t[8]<<24)|((uint32_t)t[9]<<16)|((uint32_t)t[10]<<8)|t[11];
                    *rflags = t[13];
                    int thl = (t[12] >> 4) * 4;
                    int iptot = (ip[2] << 8) | ip[3];
                    int avail = (int)len - (14 + ihl + thl);   /* bytes actually in this frame */
                    if (avail < 0) avail = 0;
                    int pl = iptot - ihl - thl;
                    if (pl < 0) pl = 0;
                    if (pl > avail) pl = avail;   /* never read past the received frame (OOB) */
                    if (pl > maxp) pl = maxp;     /* never overflow the payload buffer */
                    for (int i = 0; i < pl; i++) payload[i] = t[thl + i];
                    *plen = pl;
                    return 1;
                }
            }
        }
    }
    return 0;
}

int net_http_get_ep(const char *host, uint16_t port, const char *path, char *out, int maxout) {
    uint8_t sip[4];
    if (!net_parse_ip(host, sip) && !net_dns_resolve(host, sip)) return -1;  /* IP literal or DNS */
    uint8_t dmac[6];
    if (!net_arp_resolve(gw_ip, dmac)) return -2;       /* route via the gateway */

    uint16_t sport = 49152, dport = port;
    uint32_t myseq = 1000, theirseq = 0, seq = 0, ack = 0; uint8_t flags = 0;
    static uint8_t pl[1600]; int plen;

    int synack = 0;
    for (int attempt = 0; attempt < 3 && !synack; attempt++) {        /* retry handshake */
        rtl8139_flush();
        tcp_send(dmac, sip, sport, dport, myseq, 0, 0x02, 0, 0);      /* SYN */
        if (tcp_recv(sport, dport, &seq, &ack, &flags, pl, &plen, sizeof(pl)) && (flags & 0x12))
            synack = 1;
    }
    if (!synack) return -3;
    theirseq = seq + 1; myseq += 1;
    tcp_send(dmac, sip, sport, dport, myseq, theirseq, 0x10, 0, 0);   /* ACK */

    char req[300]; int r = 0;
    const char *g = "GET ";              while (*g)  req[r++] = *g++;
    const char *pp = path;               while (*pp) req[r++] = *pp++;
    const char *h1 = " HTTP/1.0\r\nHost: "; while (*h1) req[r++] = *h1++;
    const char *hh = host;               while (*hh) req[r++] = *hh++;
    const char *tl = "\r\nConnection: close\r\n\r\n"; while (*tl) req[r++] = *tl++;
    tcp_send(dmac, sip, sport, dport, myseq, theirseq, 0x18, (uint8_t *)req, r);  /* PSH+ACK */
    myseq += r;

    int total = 0;
    for (;;) {
        if (!tcp_recv(sport, dport, &seq, &ack, &flags, pl, &plen, sizeof(pl))) break;
        if (plen > 0 && seq == theirseq) {
            for (int i = 0; i < plen && total < maxout - 1; i++) out[total++] = (char)pl[i];
            theirseq += plen;
        }
        if (plen > 0 || (flags & 0x01))
            tcp_send(dmac, sip, sport, dport, myseq, theirseq + ((flags & 0x01) ? 1 : 0), 0x10, 0, 0);
        if (flags & 0x01) {                                          /* FIN */
            theirseq += 1;
            tcp_send(dmac, sip, sport, dport, myseq, theirseq, 0x11, 0, 0);  /* our FIN+ACK */
            break;
        }
        if (total >= maxout - 1) break;
    }
    if (total < maxout) out[total] = 0;
    return total;
}

int net_http_get(const char *host, const char *path, char *out, int maxout) {
    return net_http_get_ep(host, 80, path, out, maxout);
}
