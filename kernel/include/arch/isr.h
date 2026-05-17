#ifndef ISR_H
#define ISR_H

#include <libk/types.h>

// Register state - interrupt sırasında kaydedilen register'lar (32-bit)
struct registers {
  uint32_t ds;                                     // Data segment
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // pusha ile kaydedilen
  uint32_t int_no, err_code;             // Interrupt number ve error code
  uint32_t eip, cs, eflags, useresp, ss; // CPU tarafından push edilen
};

// ISR handler fonksiyon pointer tipi
typedef struct registers registers_t;
typedef void (*isr_t)(registers_t *);

// ISR fonksiyonları (interrupt.s'de tanımlı)
extern void isr128(void); // System Call (INT 0x80)
// Exception handlers (0-31)
extern void isr0(void);  // Division by zero
extern void isr1(void);  // Debug
extern void isr2(void);  // Non-maskable interrupt
extern void isr3(void);  // Breakpoint
extern void isr4(void);  // Overflow
extern void isr5(void);  // Bound range exceeded
extern void isr6(void);  // Invalid opcode
extern void isr7(void);  // Device not available
extern void isr8(void);  // Double fault
extern void isr9(void);  // Coprocessor segment overrun
extern void isr10(void); // Invalid TSS
extern void isr11(void); // Segment not present
extern void isr12(void); // Stack-segment fault
extern void isr13(void); // General protection fault
extern void isr14(void); // Page fault
extern void isr15(void); // Reserved
extern void isr16(void); // x87 floating-point exception
extern void isr17(void); // Alignment check
extern void isr18(void); // Machine check
extern void isr19(void); // SIMD floating-point exception
extern void isr20(void); // Virtualization exception
extern void isr21(void); // Reserved
extern void isr22(void); // Reserved
extern void isr23(void); // Reserved
extern void isr24(void); // Reserved
extern void isr25(void); // Reserved
extern void isr26(void); // Reserved
extern void isr27(void); // Reserved
extern void isr28(void); // Reserved
extern void isr29(void); // Reserved
extern void isr30(void); // Security exception
extern void isr31(void); // Reserved

// Hardware interrupt handlers (IRQ 0-15, remapped to INT 32-47)
extern void irq0(void);  // Timer
extern void irq1(void);  // Keyboard
extern void irq2(void);  // Cascade (internal)
extern void irq3(void);  // COM2
extern void irq4(void);  // COM1
extern void irq5(void);  // LPT2
extern void irq6(void);  // Floppy disk
extern void irq7(void);  // LPT1
extern void irq8(void);  // CMOS real-time clock
extern void irq9(void);  // Free
extern void irq10(void); // Free
extern void irq11(void); // Free
extern void irq12(void); // PS/2 Mouse
extern void irq13(void); // FPU
extern void irq14(void); // Primary ATA
extern void irq15(void); // Secondary ATA

// C handler fonksiyonları
registers_t *isr_handler(registers_t *regs);
registers_t *irq_handler(registers_t *regs);

// ISR handler kaydetme
void register_interrupt_handler(uint8_t n, isr_t handler);

#endif // ISR_H
