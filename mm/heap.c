/* heap.c - a simple first-fit allocator over a fixed kernel arena. Blocks carry a
 * small header; kfree marks them free and coalesces with the next block. Enough
 * for drivers and structures without needing full paging yet. */
#include "heap.h"
#include "string.h"

#define ARENA_SIZE (4 * 1024 * 1024)     /* 4 MiB kernel heap */

typedef struct block {
    size_t        size;     /* payload bytes */
    int           free;
    struct block *next;
} block_t;

static uint8_t  arena[ARENA_SIZE];
static block_t *head;
static size_t   used;

void heap_init(void) {
    head = (block_t *)arena;
    head->size = ARENA_SIZE - sizeof(block_t);
    head->free = 1;
    head->next = NULL;
    used = 0;
}

void *kmalloc(size_t n) {
    n = (n + 7) & ~7u;                   /* 8-byte align */
    for (block_t *b = head; b; b = b->next) {
        if (b->free && b->size >= n) {
            if (b->size >= n + sizeof(block_t) + 16) {   /* split */
                block_t *nb = (block_t *)((uint8_t *)b + sizeof(block_t) + n);
                nb->size = b->size - n - sizeof(block_t);
                nb->free = 1;
                nb->next = b->next;
                b->size  = n;
                b->next  = nb;
            }
            b->free = 0;
            used += b->size + sizeof(block_t);
            return (uint8_t *)b + sizeof(block_t);
        }
    }
    return NULL;                          /* out of heap */
}

void kfree(void *p) {
    if (!p) return;
    block_t *b = (block_t *)((uint8_t *)p - sizeof(block_t));
    if (b->free) return;                  /* double-free guard: avoids used-underflow + list corruption */
    b->free = 1;
    used -= b->size + sizeof(block_t);
    if (b->next && b->next->free) {       /* coalesce forward */
        b->size += sizeof(block_t) + b->next->size;
        b->next  = b->next->next;
    }
    if (b != head) {                      /* coalesce backward (fights long-uptime fragmentation) */
        block_t *prev = head;
        while (prev && prev->next != b) prev = prev->next;
        if (prev && prev->free) {
            prev->size += sizeof(block_t) + b->size;
            prev->next  = b->next;
        }
    }
}

size_t heap_used(void) { return used; }
