/* RIVET — arp.h : ARP cache + request/reply builder. */
#ifndef RIVET_ARP_H
#define RIVET_ARP_H

#include "core.h"
#include "net.h"
#include "mem.h"

#define RIV_ARP_HW_ETH      1
#define RIV_ARP_OP_REQUEST  1
#define RIV_ARP_OP_REPLY    2

typedef struct RIV_PACKED {
    riv_u16 hw_type;
    riv_u16 proto_type;
    riv_u8  hw_len;
    riv_u8  proto_len;
    riv_u16 op;
    riv_u8  sender_mac[6];
    riv_u32 sender_ip;
    riv_u8  target_mac[6];
    riv_u32 target_ip;
} riv_arp_hdr;

typedef struct {
    riv_u32 ip;          /* network byte order */
    riv_u8  mac[6];
    riv_u64 expiry_ms;
} riv_arp_entry;

#define RIV_ARP_CACHE_SIZE 16

typedef struct {
    riv_arp_entry entries[RIV_ARP_CACHE_SIZE];
} riv_arp_cache;

RIV_ALWAYS void riv_arp_cache_init(riv_arp_cache *c) {
    for (int i = 0; i < RIV_ARP_CACHE_SIZE; ++i) c->entries[i].ip = 0;
}

RIV_ALWAYS int riv_arp_lookup(const riv_arp_cache *c, riv_u32 ip, riv_u8 mac[6]) {
    for (int i = 0; i < RIV_ARP_CACHE_SIZE; ++i) {
        if (c->entries[i].ip == ip) {
            riv_memcpy(mac, c->entries[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

RIV_ALWAYS void riv_arp_insert(riv_arp_cache *c, riv_u32 ip,
                               const riv_u8 mac[6], riv_u64 expiry_ms) {
    int oldest = 0;
    riv_u64 oldest_exp = c->entries[0].expiry_ms;
    for (int i = 0; i < RIV_ARP_CACHE_SIZE; ++i) {
        if (c->entries[i].ip == ip || c->entries[i].ip == 0) {
            c->entries[i].ip = ip;
            riv_memcpy(c->entries[i].mac, mac, 6);
            c->entries[i].expiry_ms = expiry_ms;
            return;
        }
        if (c->entries[i].expiry_ms < oldest_exp) {
            oldest_exp = c->entries[i].expiry_ms;
            oldest = i;
        }
    }
    /* evict oldest */
    c->entries[oldest].ip = ip;
    riv_memcpy(c->entries[oldest].mac, mac, 6);
    c->entries[oldest].expiry_ms = expiry_ms;
}

/* Build a 42-byte ARP request frame into `out`. */
RIV_ALWAYS riv_u32 riv_arp_build_request(riv_u8 out[42], const riv_u8 src_mac[6],
                                          riv_u32 src_ip, riv_u32 target_ip) {
    riv_eth_hdr *eh = (riv_eth_hdr*)out;
    for (int i = 0; i < 6; ++i) eh->dst[i] = 0xFF;
    riv_memcpy(eh->src, src_mac, 6);
    eh->ethertype = riv_htons(RIV_ETH_TYPE_ARP);

    riv_arp_hdr *ah = (riv_arp_hdr*)(out + sizeof(riv_eth_hdr));
    ah->hw_type    = riv_htons(RIV_ARP_HW_ETH);
    ah->proto_type = riv_htons(RIV_ETH_TYPE_IPV4);
    ah->hw_len     = 6;
    ah->proto_len  = 4;
    ah->op         = riv_htons(RIV_ARP_OP_REQUEST);
    riv_memcpy(ah->sender_mac, src_mac, 6);
    ah->sender_ip  = src_ip;
    for (int i = 0; i < 6; ++i) ah->target_mac[i] = 0;
    ah->target_ip  = target_ip;
    return 42;
}

#endif /* RIVET_ARP_H */
