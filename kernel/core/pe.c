#include <drivers/vga.h>
#include <kernel/pe.h>
#include <libk/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

// PE Dosyasını belirtilen sayfa dizinine (dir) yükle ve çalıştır
bool pe_load(uint8_t *file_data, uint32_t file_size, uint32_t *entry_point,
             uint32_t *user_stack_top, struct page_directory *target_dir) {
  if (!file_data || file_size < sizeof(struct pe_dos_header) || !target_dir) {
    kprintf("PE: File too small, NULL data or NULL target_dir\n");
    return false;
  }

  struct pe_dos_header *dos_hdr = (struct pe_dos_header *)file_data;

  // MZ kontrolü
  if (dos_hdr->e_magic != 0x5A4D) {
    kprintf("PE: Invalid MZ signature (0x%x)\n", dos_hdr->e_magic);
    return false;
  }

  // PE Header adresi kontrolü
  if (dos_hdr->e_lfanew + sizeof(struct pe_header) > file_size) {
    kprintf("PE: e_lfanew out of bounds (0x%x)\n", dos_hdr->e_lfanew);
    return false;
  }

  struct pe_header *pe_hdr =
      (struct pe_header *)(file_data + dos_hdr->e_lfanew);

  // PE kontrolü
  if (pe_hdr->signature != 0x00004550) {
    kprintf("PE: Invalid PE signature (0x%x)\n", pe_hdr->signature);
    return false;
  }

  uint32_t opt_hdr_addr = dos_hdr->e_lfanew + sizeof(struct pe_header);
  if (opt_hdr_addr + pe_hdr->optional_header_size > file_size) {
    kprintf("PE: Optional header out of bounds\n");
    return false;
  }

  struct pe_optional_header *opt_hdr =
      (struct pe_optional_header *)(file_data + opt_hdr_addr);

  kprintf("PE: Entry Point: 0x%08x, Sections: %d\n", opt_hdr->entry_point,
          pe_hdr->section_count);

  uint32_t sections_addr = opt_hdr_addr + pe_hdr->optional_header_size;
  if (sections_addr +
          (pe_hdr->section_count * sizeof(struct pe_section_header)) >
      file_size) {
    kprintf("PE: Section headers out of bounds\n");
    return false;
  }

  struct pe_section_header *sections =
      (struct pe_section_header *)(file_data + sections_addr);

  // Her section'ı doğru sanal adrese yerleştir
  for (int i = 0; i < pe_hdr->section_count; i++) {
    kprintf("PE: Section %s at 0x%08x\n", sections[i].name,
            sections[i].virtual_address + opt_hdr->image_base);

    uint32_t virt_addr = sections[i].virtual_address + opt_hdr->image_base;
    uint32_t size = sections[i].virtual_size;
    uint32_t page_count = (size + 4095) / 4096;

    // Sayfa haritalama (Geçici olarak hedefe geç veya kernel haritalamalarını
    // kullan?) En temizi map yaparken, map func target almalı veya geçici
    // switch yapmalıyız. vmm.c'deki vmm_map_page şu an current_directory
    // kullanıyor.
    // Removed cli/sti because SYS_EXEC is already an interrupt handler.
    struct page_directory *prev_dir = vmm_get_kernel_directory();
    vmm_switch_directory((struct page_directory *)V2P(target_dir));

    for (uint32_t j = 0; j < page_count; j++) {
      void *phys = pmm_alloc_block();
      // MEMORY LEAK RISK: pmm_alloc_block returns GARBAGE.
      // Must zero the allocated page BEFORE passing it to user! Wait, how to
      // zero it if it's physical? Map it, then memset it!
      vmm_map_page(phys, (void *)(virt_addr + j * 4096),
                   VMM_PRESENT | VMM_WRITABLE | VMM_USER);
      memset((void *)(virt_addr + j * 4096), 0, 4096);
    }
    vmm_switch_directory((struct page_directory *)V2P(prev_dir));

    // Veri kopyalama (sınır kontrolü)
    uint32_t raw_ptr = sections[i].raw_data_pointer;
    uint32_t raw_size = sections[i].raw_data_size;
    if (raw_ptr + raw_size > file_size) {
      kprintf("PE: Section data out of bounds\n");
      return false;
    }

    if (raw_size > 0) {
      // DİKKAT: virt_addr şu an target_dir içinde geçerli.
      // Biz kernel_dir'deyken oraya MEMCPY YAPAMAYIZ!
      // Geçici olarak o dizine geçip kopyalamalıyız.
      struct page_directory *prev_dir = vmm_get_kernel_directory();
      vmm_switch_directory((struct page_directory *)V2P(target_dir));
      memcpy((void *)virt_addr, file_data + raw_ptr, raw_size);
      vmm_switch_directory((struct page_directory *)V2P(prev_dir));
    }
  }

  uint32_t entry = opt_hdr->entry_point + opt_hdr->image_base;
  *entry_point = entry;
  kprintf("PE: Loaded image, entry at 0x%08x\n", entry);

  // Allocate User Stack (e.g., 16KB at 0xB0000000)
  uint32_t user_stack_base = 0xB0000000;

  struct page_directory *prev_dir = vmm_get_kernel_directory();
  vmm_switch_directory((struct page_directory *)V2P(target_dir));
  for (int j = 0; j < 4; j++) {
    void *phys = pmm_alloc_block();
    vmm_map_page(phys, (void *)(user_stack_base + j * 4096),
                 VMM_PRESENT | VMM_WRITABLE | VMM_USER);
    memset((void *)(user_stack_base + j * 4096), 0, 4096);
  }
  vmm_switch_directory((struct page_directory *)V2P(prev_dir));

  *user_stack_top = user_stack_base + 4 * 4096;

  return true;
}
