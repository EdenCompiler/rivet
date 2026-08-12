/* RIVET — net.h : Ethernet / IPv4 / UDP / ICMP header structs + helpers.
 *
 * All fields are network-byte-order on the wire. Helpers below convert
 * to/from host byte order. Drivers send raw frames; higher layers fill
 * the structs and compute the checksums. */
#ifndef RIVET_NET_H
#define RIVET_NET_H

#include "core.h"

#define RIV_ETH_TYPE_IPV4   0x0800
#define RIV_ETH_TYPE_ARP    0x0806
#define RIV_ETH_TYPE_IPV6   0x86DD

#define RIV_IP_PROTO_ICMP   1
#define RIV_IP_PROTO_TCP    6
#define RIV_IP_PROTO_UDP    17

typedef struct RIV_PACKED {
    riv_u8  dst[6];
    riv_u8  src[6];
    riv_u16 ethertype;     /* network order */
} riv_eth_hdr;

typedef struct RIV_PACKED {
    riv_u8  ver_ihl;       /* high nibble = version (4), low = IHL words */
    riv_u8  tos;
    riv_u16 total_len;     /* network order */
    riv_u16 id;
    riv_u16 frag_off;
    riv_u8  ttl;
    riv_u8  proto;
    riv_u16 checksum;
    riv_u32 src;           /* network order */
    riv_u32 dst;
} riv_ipv4_hdr;

typedef struct RIV_PACKED {
    riv_u16 src_port;
    riv_u16 dst_port;
    riv_u16 length;
    riv_u16 checksum;
} riv_udp_hdr;

typedef struct RIV_PACKED {
    riv_u8  type;
    riv_u8  code;
    riv_u16 checksum;
    riv_u32 rest;
} riv_icmp_hdr;

/* Byte-order helpers (works on little-endian hosts; defensive on BE). */
RIV_ALWAYS riv_u16 riv_htons(riv_u16 v) {
    return (riv_u16)((v << 8) | (v >> 8));
}
RIV_ALWAYS riv_u32 riv_htonl(riv_u32 v) {
    return  ((v & 0xFF) << 24) | ((v & 0xFF00) << 8)
          | ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}
#define riv_ntohs riv_htons
#define riv_ntohl riv_htonl

/* IPv4 one's-complement checksum. Pass any 16-bit-aligned buffer. */
RIV_ALWAYS riv_u16 riv_ip_checksum(const void *data, riv_size n) {
    const riv_u16 *p = (const riv_u16*)data;
    riv_u32 sum = 0;
    while (n > 1) { sum += *p++; n -= 2; }
    if (n) sum += *(const riv_u8*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (riv_u16)~sum;
}

/* Build a dotted-quad IPv4 address as a 32-bit value in network order. */
RIV_ALWAYS riv_u32 riv_ipv4(riv_u8 a, riv_u8 b, riv_u8 c, riv_u8 d) {
    return riv_htonl(((riv_u32)a << 24) | ((riv_u32)b << 16)
                   | ((riv_u32)c <<  8) |  (riv_u32)d);
}

/* Network interface vtable — drivers provide xmit, MAC, and an rx
 * upcall hook. */
typedef struct riv_netif riv_netif;
struct riv_netif {
    const char *name;
    riv_u8      mac[6];
    int  (*xmit)(riv_netif *self, const void *frame, riv_u32 n);
    void (*on_rx)(riv_netif *self, const void *frame, riv_u32 n);
    void *priv;
};

#endif /* RIVET_NET_H */
