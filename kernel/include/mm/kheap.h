#ifndef KHEAP_H
#define KHEAP_H

#include <libk/types.h>

// Basit bir heap allocator
void kheap_init(uint32_t start_addr, size_t size);
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif // KHEAP_H
