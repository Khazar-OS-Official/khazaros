#ifndef IDT_H
#define IDT_H

#include <libk/types.h>

// IDT Entry (Gate Descriptor) - 8 bytes (32-bit)
struct idt_entry {
  uint16_t base_low;  // Handler address bits 0-15
  uint16_t selector;  // GDT code segment selector
  uint8_t always0;    // Always 0
  uint8_t flags;      // Gate type ve DPL
  uint16_t base_high; // Handler address bits 16-31
} __attribute__((packed));

// IDT Pointer - IDTR register için (32-bit)
struct idt_ptr {
  uint16_t limit; // IDT size - 1
  uint32_t base;  // IDT başlangıç adresi (32-bit)
} __attribute__((packed));

// IDT Flags
#define IDT_FLAG_PRESENT 0x80    // Segment present
#define IDT_FLAG_RING0 0x00      // Ring 0 (kernel)
#define IDT_FLAG_RING3 0x60      // Ring 3 (user)
#define IDT_FLAG_GATE_32BIT 0x0E // 32-bit interrupt gate

// PIC (8259) ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

// PIC commands
#define PIC_EOI 0x20 // End of Interrupt

// Functions
void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);

// Assembly function (interrupt.s)
extern void idt_flush(uint32_t idt_ptr);

#endif // IDT_H
