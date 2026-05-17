#ifndef PE_H
#define PE_H

#include <libk/types.h>

// PE Header Yapıları
struct pe_dos_header {
  uint16_t e_magic; // "MZ"
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovno;
  uint16_t e_res[4];
  uint16_t e_oemid;
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  uint32_t e_lfanew; // PE Header adresi
} __attribute__((packed));

struct pe_header {
  uint32_t signature; // "PE\0\0"
  uint16_t machine;
  uint16_t section_count;
  uint32_t timestamp;
  uint32_t symbol_table;
  uint32_t symbol_count;
  uint16_t optional_header_size;
  uint16_t characteristics;
} __attribute__((packed));

struct pe_optional_header {
  uint16_t magic;
  uint8_t linker_major;
  uint8_t linker_minor;
  uint32_t code_size;
  uint32_t data_size;
  uint32_t uninit_data_size;
  uint32_t entry_point;
  uint32_t code_base;
  uint32_t data_base;
  uint32_t image_base;
  uint32_t section_alignment;
  uint32_t file_alignment;
  // ... Diğer alanlar şimdilik opsiyonel
} __attribute__((packed));

struct pe_section_header {
  char name[8];
  uint32_t virtual_size;
  uint32_t virtual_address;
  uint32_t raw_data_size;
  uint32_t raw_data_pointer;
  uint32_t reloc_pointer;
  uint32_t linenum_pointer;
  uint16_t reloc_count;
  uint16_t linenum_count;
  uint32_t characteristics;
} __attribute__((packed));

// PE Fonksiyonları
struct page_directory;
bool pe_load(uint8_t *file_data, uint32_t file_size, uint32_t *entry_point,
             uint32_t *user_stack_top, struct page_directory *target_dir);

#endif // PE_H
