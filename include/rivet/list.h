/* RIVET — list.h : intrusive circular doubly-linked list.
 * Embed `riv_node` in your own struct; use riv_container_of via CN macros. */
#ifndef RIVET_LIST_H
#define RIVET_LIST_H

#include "core.h"

typedef struct riv_node {
    struct riv_node *prev, *next;
} riv_node;

#define RIV_LIST_HEAD(name) riv_node name = { &(name), &(name) }

RIV_ALWAYS void riv_list_init(riv_node *h) { h->prev = h; h->next = h; }

RIV_ALWAYS int  riv_list_empty(const riv_node *h) { return h->next == h; }

RIV_ALWAYS void riv_list_insert(riv_node *prev, riv_node *next, riv_node *n) {
    n->prev = prev; n->next = next;
    prev->next = n; next->prev = n;
}
RIV_ALWAYS void riv_list_add (riv_node *h, riv_node *n) { riv_list_insert(h, h->next, n); }
RIV_ALWAYS void riv_list_add_tail(riv_node *h, riv_node *n) { riv_list_insert(h->prev, h, n); }

RIV_ALWAYS void riv_list_del(riv_node *n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->prev = n->next = n;
}

/* pop head/tail; returns NULL if empty */
RIV_ALWAYS riv_node *riv_list_pop(riv_node *h) {
    if (riv_list_empty(h)) return (riv_node*)0;
    riv_node *n = h->next; riv_list_del(n); return n;
}
RIV_ALWAYS riv_node *riv_list_pop_tail(riv_node *h) {
    if (riv_list_empty(h)) return (riv_node*)0;
    riv_node *n = h->prev; riv_list_del(n); return n;
}

/* iteration macros */
#define riv_list_for_each(it, h) \
    for (riv_node *it = (h)->next; it != (h); it = it->next)

#define riv_list_for_each_safe(it, tmp, h) \
    for (riv_node *it = (h)->next, *tmp = it->next; \
         it != (h); it = tmp, tmp = it->next)

#define riv_list_entry(ptr, type, member) RIV_CONTAINER_OF(ptr, type, member)

#endif /* RIVET_LIST_H */
