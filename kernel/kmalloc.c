#include "kmalloc.h"
#include <stdint.h>

#define HEAP_BASE     ((uint8_t *)0x00100000u)
#define HEAP_SIZE     (512u * 1024u)

static uint8_t *heap_next;

void kmalloc_init(void) {
    heap_next = HEAP_BASE;
}

void *kmalloc(size_t size) {
    if (size == 0)
        return NULL;

    uintptr_t p = (uintptr_t)heap_next;
    p = (p + 15u) & ~(uintptr_t)15u;
    if (p + size > (uintptr_t)HEAP_BASE + HEAP_SIZE)
        return NULL;

    heap_next = (uint8_t *)(p + size);
    return (void *)p;
}

size_t kmalloc_used(void) {
    return (size_t)(heap_next - HEAP_BASE);
}

size_t kmalloc_capacity(void) {
    return HEAP_SIZE;
}
