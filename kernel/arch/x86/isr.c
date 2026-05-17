#include <arch/idt.h>
#include <arch/io.h>
#include <arch/isr.h>
#include <drivers/vga.h>
#include <proc/process.h>

#include <kernel/panic.h>

// ISR handler array - custom handler'lar için
static isr_t interrupt_handlers[256];

// Exception mesajları
static const char *exception_messages[] = {"Division By Zero",
                                           "Debug",
                                           "Non Maskable Interrupt",
                                           "Breakpoint",
                                           "Overflow",
                                           "Bound Range Exceeded",
                                           "Invalid Opcode",
                                           "Device Not Available",
                                           "Double Fault",
                                           "Coprocessor Segment Overrun",
                                           "Invalid TSS",
                                           "Segment Not Present",
                                           "Stack-Segment Fault",
                                           "General Protection Fault",
                                           "Page Fault",
                                           "Reserved",
                                           "x87 Floating-Point Exception",
                                           "Alignment Check",
                                           "Machine Check",
                                           "SIMD Floating-Point Exception",
                                           "Virtualization Exception",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Security Exception",
                                           "Reserved"};

// Custom interrupt handler kaydet
void register_interrupt_handler(uint8_t n, isr_t handler) {
  interrupt_handlers[n] = handler;
}

// Exception handler (ISR 0-31)
registers_t *isr_handler(registers_t *regs) {
  // Custom handler varsa çağır
  if (interrupt_handlers[regs->int_no] != 0) {
    isr_t handler = interrupt_handlers[regs->int_no];
    handler(regs);
  } else {
    // Default exception handler - ekrana yazdır (32-bit)
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    kprintf("\n!!! EXCEPTION: %s !!!\n", exception_messages[regs->int_no]);
    kprintf("INT: %d, ERR: 0x%x\n", regs->int_no, regs->err_code);
    kprintf("EIP: 0x%x, CS: 0x%x, EFLAGS: 0x%x\n", regs->eip, regs->cs,
            regs->eflags);

    // Sistem durdur (Eğer kernel task'ta ise)
    thread_t *thread = scheduler_get_current_thread();
    process_t *proc = thread ? thread->process : NULL;
    if (proc && proc->pid > 0) {
      kprintf(
          "\nUser Thread %d (Process %d) caused Exception %d! Terminating.\n",
          thread->tid, proc->pid, regs->int_no);
      thread->state = THREAD_STATE_TERMINATED;
      return scheduler_schedule(regs);
    } else {
      char panic_msg[128];
      if (regs->int_no < 32) {
          // exception_messages kullanarak xeta adini yazdiralim
          // sprintf olmadigi ucun, biz sadece basic panic isledeceyik
          // ksprintf is available in libk? we can just call panic directly.
      }
      PANIC(exception_messages[regs->int_no]);
      __asm__ __volatile__("cli; hlt");
    }
  }

  return scheduler_schedule(regs);
}

// Hardware interrupt handler (IRQ 0-15, INT 32-47)
registers_t *irq_handler(registers_t *regs) {
  // Handle spurious interrupts (IRQ 7 and IRQ 15)
  if (regs->int_no == 39) { // IRQ 7
      outb(PIC1_COMMAND, 0x0B); 
      uint8_t isr = inb(PIC1_COMMAND);
      if (!(isr & 0x80)) {
          return scheduler_schedule(regs); // Spurious, no EOI
      }
  }

  if (regs->int_no == 47) { // IRQ 15
      outb(PIC2_COMMAND, 0x0B);
      uint8_t isr = inb(PIC2_COMMAND);
      if (!(isr & 0x80)) {
          outb(PIC1_COMMAND, PIC_EOI); // Send EOI to master only
          return scheduler_schedule(regs);
      }
  }

  // EOI (End of Interrupt) gönder
  if (regs->int_no >= 40) {
    outb(PIC2_COMMAND, PIC_EOI);
  }
  outb(PIC1_COMMAND, PIC_EOI);

  // Custom handler varsa çağır
  if (interrupt_handlers[regs->int_no] != 0) {
    isr_t handler = interrupt_handlers[regs->int_no];
    handler(regs);
  }

  return scheduler_schedule(regs);
}
