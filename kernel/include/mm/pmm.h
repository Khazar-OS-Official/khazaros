#ifndef PMM_H
#define PMM_H

#include <libk/types.h>

#define PMM_BLOCK_SIZE 4096
#define PMM_BLOCKS_PER_BYTE 8

// PMM Fonksiyonları
void pmm_init(size_t mem_size, uint32_t bitmap_addr);
void pmm_init_region(uint32_t base, size_t size);
void pmm_deinit_region(uint32_t base, size_t size);

void *pmm_alloc_block(void);
void pmm_free_block(void *p);

// Yardımcılar
size_t pmm_get_block_count(void);
size_t pmm_get_free_block_count(void);

#endif // PMM_H
