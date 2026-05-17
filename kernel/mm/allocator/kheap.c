#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <drivers/vga.h>
#include <libk/string.h>

/* 
 * Khazar OS - Gelişmiş Heap Allocator (Linked List)
 * Bu versiya 'kfree' destekler ve boş blokları birleştirir (coalescing).
 */

typedef struct header {
    uint32_t magic;     // Hatalı free işlemlerini yakalamak için
    uint32_t size;      // Blok boyutu (header hariç)
    uint8_t is_free;    // 1: Boş, 0: Dolu
    struct header *next;
    struct header *prev;
} header_t;

#define KHEAP_MAGIC 0x1234ABCD
#define HEADER_SIZE sizeof(header_t)

static header_t *heap_start = NULL;

void kheap_init(uint32_t start_addr, size_t size) {
    // Önce fiziksel sayfaları haritalayalım (kernel.c'deki gibi)
    // Not: Fiziksel bloklar 0x500000'den başlıyor (PMM'de rezerve edildi)
    uint32_t phys = 0x500000;
    for (uint32_t v = start_addr; v < start_addr + size; v += 4096) {
        if (vmm_get_phys((void *)v) == NULL) {
            vmm_map_page((void *)phys, (void *)v, VMM_PRESENT | VMM_WRITABLE);
            phys += 4096;
        }
    }

    // İlk büyük bloku oluştur
    heap_start = (header_t *)start_addr;
    heap_start->magic = KHEAP_MAGIC;
    heap_start->size = size - HEADER_SIZE;
    heap_start->is_free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    kprintf("KHEAP: Advanced Allocator ready at 0x%x, size: %d KB\n", start_addr, size / 1024);
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // 8-byte hizalama
    size = (size + 7) & ~7;

    header_t *curr = heap_start;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            // Bloku bulduk. Eğer blok çok büyükse ikiye bölelim (splitting)
            if (curr->size >= size + HEADER_SIZE + 8) {
                header_t *new_block = (header_t *)((uint32_t)curr + HEADER_SIZE + size);
                new_block->magic = KHEAP_MAGIC;
                new_block->size = curr->size - size - HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next = curr->next;
                new_block->prev = curr;

                if (curr->next) {
                    curr->next->prev = new_block;
                }

                curr->next = new_block;
                curr->size = size;
            }

            curr->is_free = 0;
            return (void *)((uint32_t)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }

    kprintf("KHEAP: OUT OF MEMORY (Request: %d bytes)!\n", size);
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;

    header_t *header = (header_t *)((uint32_t)ptr - HEADER_SIZE);

    // Güvenlik kontrolü
    if (header->magic != KHEAP_MAGIC) {
        kprintf("KHEAP ERROR: Invalid kfree(0x%x) - Magic mismatch!\n", ptr);
        return;
    }

    if (header->is_free) {
        kprintf("KHEAP ERROR: Double free detected at 0x%x!\n", ptr);
        return;
    }

    header->is_free = 1;

    // COALESCING (Blokları Birleştirme)
    // 1. Sonraki blok boş mu?
    if (header->next && header->next->is_free) {
        header->size += HEADER_SIZE + header->next->size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    // 2. Önceki blok boş mu?
    if (header->prev && header->prev->is_free) {
        header->prev->size += HEADER_SIZE + header->size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
    }
}
