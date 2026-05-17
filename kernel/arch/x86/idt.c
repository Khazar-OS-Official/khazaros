#include <arch/idt.h>
#include <arch/io.h>
#include <arch/isr.h>
#include <libk/string.h>

// IDT entries - 256 interrupt descriptors
struct idt_entry idt_entries[256];
struct idt_ptr idt_pointer;

// IDT gate ayarla (descriptor oluştur) - 32-bit
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector,
                  uint8_t flags) {
  idt_entries[num].base_low = base & 0xFFFF;
  idt_entries[num].base_high = (base >> 16) & 0xFFFF;
  idt_entries[num].selector = selector;
  idt_entries[num].always0 = 0;
  idt_entries[num].flags = flags;
}

// PIC'i remap et (IRQ 0-15 → INT 32-47)
static void pic_remap(void) {
  // ICW1: Initialize
  outb(PIC1_COMMAND, 0x11);
  outb(PIC2_COMMAND, 0x11);
  io_wait();

  // ICW2: Remap offsets
  outb(PIC1_DATA, 0x20); // Master PIC: IRQ 0-7  → INT 32-39
  outb(PIC2_DATA, 0x28); // Slave PIC:  IRQ 8-15 → INT 40-47
  io_wait();

  // ICW3: Cascade identity
  outb(PIC1_DATA, 0x04); // Master: IRQ2'de slave var
  outb(PIC2_DATA, 0x02); // Slave: Cascade identity 2
  io_wait();

  // ICW4: 8086 mode
  outb(PIC1_DATA, 0x01);
  outb(PIC2_DATA, 0x01);
  io_wait();

  // Mask all interrupts (başlangıçta hepsi kapalı)
  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);
}

// IDT'yi başlat (32-bit)
void idt_init(void) {
  // IDT pointer'ı ayarla
  idt_pointer.limit = (sizeof(struct idt_entry) * 256) - 1;
  idt_pointer.base = (uint32_t)&idt_entries;

  // IDT'yi temizle
  memset(&idt_entries, 0, sizeof(struct idt_entry) * 256);

  // PIC'i remap et
  pic_remap();

  // Exception handlers (ISR 0-31)
  idt_set_gate(0, (uint32_t)isr0, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(1, (uint32_t)isr1, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(2, (uint32_t)isr2, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(3, (uint32_t)isr3, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(4, (uint32_t)isr4, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(5, (uint32_t)isr5, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(6, (uint32_t)isr6, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(7, (uint32_t)isr7, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(8, (uint32_t)isr8, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(9, (uint32_t)isr9, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(10, (uint32_t)isr10, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(11, (uint32_t)isr11, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(12, (uint32_t)isr12, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(13, (uint32_t)isr13, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(14, (uint32_t)isr14, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(15, (uint32_t)isr15, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(16, (uint32_t)isr16, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(17, (uint32_t)isr17, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(18, (uint32_t)isr18, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(19, (uint32_t)isr19, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(20, (uint32_t)isr20, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(21, (uint32_t)isr21, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(22, (uint32_t)isr22, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(23, (uint32_t)isr23, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(24, (uint32_t)isr24, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(25, (uint32_t)isr25, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(26, (uint32_t)isr26, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(27, (uint32_t)isr27, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(28, (uint32_t)isr28, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(29, (uint32_t)isr29, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(30, (uint32_t)isr30, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(31, (uint32_t)isr31, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);

  // Hardware interrupt handlers (IRQ 0-15, INT 32-47)
  idt_set_gate(32, (uint32_t)irq0, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(33, (uint32_t)irq1, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(34, (uint32_t)irq2, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(35, (uint32_t)irq3, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(36, (uint32_t)irq4, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(37, (uint32_t)irq5, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(38, (uint32_t)irq6, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(39, (uint32_t)irq7, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(40, (uint32_t)irq8, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(41, (uint32_t)irq9, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(42, (uint32_t)irq10, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(43, (uint32_t)irq11, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(44, (uint32_t)irq12, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(45, (uint32_t)irq13, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(46, (uint32_t)irq14, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);
  idt_set_gate(47, (uint32_t)irq15, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT);

  // System Call (INT 0x80) - Entry 128
  idt_set_gate(0x80, (uint32_t)isr128, 0x08,
               IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_FLAG_GATE_32BIT);

  // IDT'yi yükle (32-bit)
  idt_flush((uint32_t)&idt_pointer);
}
