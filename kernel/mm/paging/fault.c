#include <arch/isr.h>
#include <drivers/vga.h>
#include <libk/string.h>
#include <mm/paging/fault.h>
#include <proc/process.h>
#include <kernel/panic.h>

static void page_fault_handler(registers_t *regs) {
  // A page fault has occurred.
  // The faulting address is stored in the CR2 register.
  uint32_t faulting_address;
  __asm__ __volatile__("mov %%cr2, %0" : "=r"(faulting_address));

  // The error code gives us details of what happened.
  int present = !(regs->err_code & 0x1); // Page not present
  int rw = regs->err_code & 0x2;         // Write operation?
  int us = regs->err_code & 0x4;         // Processor was in user-mode?

  thread_t *current = scheduler_get_current_thread();
  process_t *proc = current ? current->process : NULL;

  // If this happened in Ring 3 (User mode) or while touching User/Kernel space illicitly
  if (us || (current && proc && (faulting_address < 0x00400000 || faulting_address > 0xBFFFFFFF))) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    kprintf("\n[SEGFAULT] Process ID %d (%s) crashed!\n",
            proc ? proc->pid : 0, proc ? proc->name : "Unknown");
    kprintf("Reason: ");
    if (faulting_address < 0x00400000 || faulting_address > 0xBFFFFFFF)
      kprintf("Out of bounds User-Pointer ");
    else if (present)
      kprintf("Page not present ");
    else if (rw)
      kprintf("Read-only mapped ");
    kprintf("at address 0x%08x\n", faulting_address);
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

    // Terminate the entire process with SIGSEGV (11)
    if (proc) {
      extern void process_terminate(process_t *proc, int status);
      process_terminate(proc, 128 + 11);
    } else if (current) {
      current->state = THREAD_STATE_TERMINATED;
    }
    return;
  }

  // If we reach here, a PAGE FAULT happened IN RING 0 (Kernel Panic!)
  char panic_msg[128];
  ksprintf(panic_msg, "PAGE FAULT at 0x%08x (Present: %d, RW: %d, UserMode: %d)",
           faulting_address, !present, rw, us);
  kernel_panic(panic_msg, __FILE__, __LINE__);
}

void fault_init(void) {
  register_interrupt_handler(14, page_fault_handler);
  kprintf("VMM: Page Fault Handler (ISR 14) installed.\n");
}
