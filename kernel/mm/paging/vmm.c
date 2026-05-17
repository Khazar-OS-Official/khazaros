#include <drivers/vga.h>
#include <libk/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

// Kernel'ın ana sayfa dizini (Kanal adresleri)
static struct page_directory *kernel_directory = NULL;
static struct page_directory *current_directory = NULL;

// VMM'i başlat
bool vmm_init(void) {
  // Page directory için fiziksel bellek ayır
  void *phys_pd = pmm_alloc_block();
  if (!phys_pd)
    return false;

  // Kernel'ın erişebileceği sanal adrese çevir
  kernel_directory = (struct page_directory *)P2V(phys_pd);
  memset(kernel_directory, 0, sizeof(struct page_directory));
  current_directory = kernel_directory;

  // 1. Identity map ilk 1MB (BIOS, VGA, Stack vs. için)
  // VMM_PRESENT zorunlu - onsuz page fault olur
  for (uint32_t i = 0; i < 0x100000; i += 4096) {
    vmm_map_page((void *)i, (void *)i, VMM_PRESENT | VMM_WRITABLE);
  }

  // 2. Higher half map kernel (0xC0000000'den itibaren ilk 128MB)
  // Boot.asm 128MB veriyor, biz de öyle yapalım
  for (uint32_t i = 0; i < 0x8000000; i += 4096) {
    vmm_map_page((void *)i, (void *)(KERNEL_VIRT_BASE + i), VMM_PRESENT | VMM_WRITABLE);
  }

  // Yeni sayfa dizinine geç (CR3 fiziksel adres ister)
  vmm_switch_directory((struct page_directory *)phys_pd);

  kprintf("VMM: Initialized. Page Directory at (Phys): 0x%x, (Virt): 0x%x\n",
          phys_pd, kernel_directory);
  return true;
}

// Sanal adresi fiziksel adrese haritala
bool vmm_map_page(void *phys, void *virt, uint32_t flags) {
  uint32_t pd_index = (uint32_t)virt >> 22;
  uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

  pd_entry *pde = &current_directory->entries[pd_index];
  struct page_table *pt;

  // Eğer sayfa tablosu henüz yoksa oluştur
  if (!(*pde & VMM_PRESENT)) {
    void *phys_pt = pmm_alloc_block();
    if (!phys_pt)
      return false;

    pt = (struct page_table *)P2V(phys_pt);
    memset(pt, 0, 4096);
    *pde = (uint32_t)phys_pt | flags | VMM_PRESENT;
  } else {
    pt = (struct page_table *)P2V(*pde & ~0xFFF);
  }

  pt_entry *pte = &pt->entries[pt_index];
  *pte = (uint32_t)phys | flags | VMM_PRESENT;

  // TLB'yi temizle
  __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");

  return true;
}

void vmm_unmap_page(void *virt) {
  uint32_t pd_index = (uint32_t)virt >> 22;
  uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

  pd_entry *pde = &current_directory->entries[pd_index];
  if (!(*pde & VMM_PRESENT))
    return;

  struct page_table *pt = (struct page_table *)P2V(*pde & ~0xFFF);
  pt->entries[pt_index] = 0;

  __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_switch_directory(struct page_directory *dir) {
  // dir fiziksel adres olmalı!
  current_directory = (struct page_directory *)P2V((uint32_t)dir);
  __asm__ __volatile__("mov %0, %%cr3" : : "r"(dir));
}

void *vmm_get_phys(void *virt) {
  uint32_t pd_index = (uint32_t)virt >> 22;
  uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

  pd_entry *pde = &current_directory->entries[pd_index];
  if (!(*pde & VMM_PRESENT))
    return NULL;

  struct page_table *pt = (struct page_table *)P2V(*pde & ~0xFFF);
  pt_entry *pte = &pt->entries[pt_index];

  if (!(*pte & VMM_PRESENT))
    return NULL;

  return (void *)(*pte & ~0xFFF);
}

struct page_directory *vmm_get_kernel_directory(void) {
  return kernel_directory;
}

struct page_directory *vmm_clone_directory(void) {
  // 1. Yeni bir page directory için fiziksel sayfa ayır
  void *phys_dir = pmm_alloc_block();
  if (!phys_dir)
    return NULL;

  struct page_directory *new_dir = (struct page_directory *)P2V(phys_dir);
  memset(new_dir, 0, sizeof(struct page_directory));

  // 2. Kernel'in Page Directory'sinden > 0xC0000000 (yani son 256 entry)
  // verileri kopyala 0xC0000000 / 0x400000 = 768. Entry'den başlar (3GB
  // noktası)
  for (int i = 768; i < 1024; i++) {
    // Sadece dizin seviyesini kopyalıyoruz, kernel memory statik ve hep
    // ortaktır
    new_dir->entries[i] = kernel_directory->entries[i];
  }

  return new_dir;
}

void vmm_free_directory(void *dir) {
  if (!dir) return;
  struct page_directory *virt_dir = (struct page_directory *)P2V(dir);

  // Sadece user-space page table'larını temizle (ilk 768 entry)
  for (int i = 0; i < 768; i++) {
    if (virt_dir->entries[i] & VMM_PRESENT) {
      void *phys_pt = (void *)(virt_dir->entries[i] & ~0xFFF);
      pmm_free_block(phys_pt);
    }
  }

  // Dizin kendisini temizle
  pmm_free_block(dir);
}
