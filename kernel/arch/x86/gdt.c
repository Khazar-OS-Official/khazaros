#include <arch/gdt.h>
#include <libk/string.h>

// GDT entries - 6 descriptors (Null, KCode, KData, UCode, UData, TSS)
struct gdt_entry gdt_entries[6];
struct gdt_ptr gdt_pointer;
tss_entry_t tss_entry;

// GDT gate ayarla (descriptor oluştur) - 32-bit
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access,
                  uint8_t gran) {
  // Base address (32-bit)
  gdt_entries[num].base_low = (base & 0xFFFF);
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].base_high = (base >> 24) & 0xFF;

  // Limit (20-bit) - 2 parçaya böl
  gdt_entries[num].limit_low = (limit & 0xFFFF);
  gdt_entries[num].granularity = (limit >> 16) & 0x0F;

  // Granularity flags (üst 4 bit)
  gdt_entries[num].granularity |= gran & 0xF0;

  // Access byte
  gdt_entries[num].access = access;
}

// TSS değerlerini yaz
void gdt_write_tss(int num, uint16_t ss0, uint32_t esp0) {
  uint32_t base = (uint32_t)&tss_entry;
  uint32_t limit = sizeof(tss_entry_t);

  // GDT'ye TSS descriptor ekle
  gdt_set_gate(num, base, limit, 0xE9, 0x00); // Present | Ring 3 | System | TSS

  // TSS yapısını temizle ve ilk değerleri ata
  memset(&tss_entry, 0, sizeof(tss_entry_t));
  tss_entry.ss0 = ss0;
  tss_entry.esp0 = esp0;

  // CS, DS, ES, FS, GS Userland'de iken kernel stack'e geçmek için gerekenler
  tss_entry.cs = 0x0B; // 0x08 | 0x03 (RPL 3)
  tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs =
      0x13; // 0x10 | 0x03
}

// Kernel stack pointerını güncelle (Scheduler tarafından kullanılır)
void set_kernel_stack(uint32_t stack) { tss_entry.esp0 = stack; }

// GDT'yi başlat
void gdt_init(void) {
  // GDT pointer'ı ayarla (32-bit)
  gdt_pointer.limit = (sizeof(struct gdt_entry) * 6) - 1;
  gdt_pointer.base = (uint32_t)&gdt_entries;

  // Flat memory model - tüm segmentler 0x00000000'dan başlar, 4GB limit

  // Entry 0: Null descriptor (zorunlu)
  gdt_set_gate(0, 0, 0, 0, 0);

  // Entry 1: Kernel Code Segment
  // Base: 0x00000000, Limit: 0xFFFFFFFF (4GB)
  // Access: Present | Ring 0 | Executable | Readable
  // Gran: 4KB granularity | 32-bit
  gdt_set_gate(1, 0, 0xFFFFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_DESCRIPTOR_TYPE |
                   GDT_ACCESS_RING0 | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_32BIT);

  // Entry 2: Kernel Data Segment
  // Base: 0x00000000, Limit: 0xFFFFFFFF (4GB)
  // Access: Present | Ring 0 | Writable
  // Gran: 4KB granularity | 32-bit
  gdt_set_gate(2, 0, 0xFFFFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_DESCRIPTOR_TYPE |
                   GDT_ACCESS_RING0 | GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_32BIT);

  // Entry 3: User Code Segment (Ring 3)
  gdt_set_gate(3, 0, 0xFFFFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_DESCRIPTOR_TYPE |
                   GDT_ACCESS_RING3 | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_32BIT);

  gdt_set_gate(4, 0, 0xFFFFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_DESCRIPTOR_TYPE |
                   GDT_ACCESS_RING3 | GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_32BIT);

  // Entry 5: TSS
  gdt_write_tss(5, 0x10, 0); // SS0 = 0x10 (Kernel Data), ESP0 geçici 0

  // GDT'yi yükle (assembly function) - 32-bit
  gdt_flush((uint32_t)&gdt_pointer);
  tss_flush(); // Hardware task register'ı yükle
}
