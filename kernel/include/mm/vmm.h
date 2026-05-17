#ifndef VMM_H
#define VMM_H

#include <libk/types.h>

// Page Directory ve Page Table entry flagleri
#define VMM_PRESENT 0x1
#define VMM_WRITABLE 0x2
#define VMM_USER 0x4
#define VMM_WRITE_THROUGH 0x08
#define VMM_CACHE_DISABLE 0x10

typedef uint32_t pd_entry;
typedef uint32_t pt_entry;

// Sayfa dizini (Page Directory) - 1024 entry (4KB)
struct page_directory {
  pd_entry entries[1024];
} __attribute__((aligned(4096)));

// Sayfa tablosu (Page Table) - 1024 entry (4KB)
struct page_table {
  pt_entry entries[1024];
} __attribute__((aligned(4096)));

// Higher Half Offset (32-bit: 3GB)
#define KERNEL_VIRT_BASE 0xC0000000

// Fiziksel -> Sanal (Kernel) - 32-bit
#define P2V(addr) ((void *)((uint32_t)(addr) + KERNEL_VIRT_BASE))
// Sanal -> Fiziksel - 32-bit
#define V2P(addr) ((uint32_t)(addr) - KERNEL_VIRT_BASE)

// VMM Fonksiyonları
bool vmm_init(void);
bool vmm_map_page(void *phys, void *virt, uint32_t flags);
void vmm_unmap_page(void *virt);
void vmm_switch_directory(struct page_directory *dir);
void *vmm_get_phys(void *virt);
struct page_directory *vmm_clone_directory(void);
void vmm_free_directory(void *dir);
struct page_directory *vmm_get_kernel_directory(void);

#endif // VMM_H
