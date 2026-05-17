#ifndef GDT_H
#define GDT_H

#include <libk/types.h>

// GDT Entry (Descriptor) - 8 bytes
struct gdt_entry {
  uint16_t limit_low;  // Limit 0-15 bits
  uint16_t base_low;   // Base 0-15 bits
  uint8_t base_middle; // Base 16-23 bits
  uint8_t access;      // Access flags
  uint8_t granularity; // Granularity + Limit 16-19 bits
  uint8_t base_high;   // Base 24-31 bits
} __attribute__((packed));

// GDT Pointer - GDTR register için (32-bit)
struct gdt_ptr {
  uint16_t limit; // GDT size - 1
  uint32_t base;  // GDT başlangıç adresi (32-bit)
} __attribute__((packed));

// TSS Entry - 104 bytes
struct tss_entry {
  uint32_t prev_tss; // Previous TSS (if hardware task linking used)
  uint32_t esp0;     // Stack pointer for Ring 0
  uint32_t ss0;      // Stack segment for Ring 0
  uint32_t esp1;
  uint32_t ss1;
  uint32_t esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;
  uint32_t cs;
  uint32_t ss;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
  uint32_t ldt;
  uint16_t trap;
  uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry tss_entry_t;

// GDT Access Byte Flags
#define GDT_ACCESS_PRESENT 0x80 // Segment present
#define GDT_ACCESS_RING0 0x00   // Ring 0 (kernel)
#define GDT_ACCESS_RING3 0x60   // Ring 3 (user)
#define GDT_ACCESS_DESCRIPTOR_TYPE                                             \
  0x10                             // Bit 4 - 1=code/data, 0=system (e.g. TSS)
#define GDT_ACCESS_EXECUTABLE 0x08 // Code segment
#define GDT_ACCESS_RW 0x02         // Readable (code) / Writable (data)
#define GDT_ACCESS_TSS 0x09 // System Segment Type: TSS (Available 32-bit)

// GDT Granularity Flags
#define GDT_GRAN_4K 0x80    // 4KB granularity
#define GDT_GRAN_32BIT 0x40 // 32-bit protected mode (D/B flag, bit 6)

// GDT Segment Selectors (offset in GDT)
#define GDT_KERNEL_CODE 0x08 // Kernel code segment (entry 1)
#define GDT_KERNEL_DATA 0x10 // Kernel data segment (entry 2)
#define GDT_USER_CODE 0x18   // User code segment (entry 3)
#define GDT_USER_DATA 0x20   // User data segment (entry 4)
#define GDT_TSS 0x28         // TSS segment (entry 5)

// Functions
void gdt_init(void);
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access,
                  uint8_t gran);
void gdt_write_tss(int num, uint16_t ss0, uint32_t esp0);
void set_kernel_stack(uint32_t stack);

// Assembly functions (gdt_flush.s)
extern void gdt_flush(uint32_t gdt_ptr);
extern void tss_flush(void);

#endif // GDT_H
