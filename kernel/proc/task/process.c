#include <arch/gdt.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <libk/string.h>
#include <mm/kheap.h>
#include <mm/vmm.h>
#include <proc/process.h>

// Ring 3'e geçiş (IRET numarasıyla)
void enter_user_mode(uint32_t entry_point, uint32_t user_stack) {
  __asm__ volatile("cli");

  // Gelecekteki interrupt'lar (örn. Syscall) için TSS'i güncelle
  thread_t *current = scheduler_get_current_thread();
  if (current && current->kernel_stack) {
    set_kernel_stack((uint32_t)current->kernel_stack + KERNEL_STACK_SIZE);
  } else {
    // Fallback for safety during bootstrap
    static void *fallback_stack = NULL;
    if (!fallback_stack)
      fallback_stack = kmalloc(KERNEL_STACK_SIZE);
    set_kernel_stack((uint32_t)fallback_stack + KERNEL_STACK_SIZE);
  }

  // User Data Segment: 0x20 | 3 = 0x23
  // User Code Segment: 0x18 | 3 = 0x1B
  __asm__ volatile("mov $0x23, %%ax\n"
                   "mov %%ax, %%ds\n"
                   "mov %%ax, %%es\n"
                   "mov %%ax, %%fs\n"
                   "mov %%ax, %%gs\n"

                   "pushl $0x23\n" // SS3
                   "pushl %0\n"    // ESP3
                   "pushfl\n"      // EFLAGS
                   "popl %%eax\n"
                   "orl $0x200, %%eax\n" // Enable interrupts (IF flag)
                   "pushl %%eax\n"
                   "pushl $0x1B\n" // CS3
                   "pushl %1\n"    // EIP3
                   "iret\n"
                   :
                   : "r"(user_stack), "r"(entry_point)
                   : "%eax");
}

static uint32_t next_pid = 0;
static uint32_t next_tid = 0;
static process_t *process_list = NULL;

void process_init(void) {
  kprintf("Process: Initializing Process Management (Unified Threads)...\n");
  next_pid = 1;
  next_tid = 1;

  // Create kernel process
  process_t *kernel_proc = (process_t *)kmalloc(sizeof(process_t));
  memset(kernel_proc, 0, sizeof(process_t));
  kernel_proc->pid = 0;
  kstrncpy(kernel_proc->name, "Kernel", 31);

  // Create kernel main thread
  thread_t *kernel_thread = (thread_t *)kmalloc(sizeof(thread_t));
  memset(kernel_thread, 0, sizeof(thread_t));
  kernel_thread->tid = 0;
  kernel_thread->state = THREAD_STATE_RUNNING;
  kernel_thread->process = kernel_proc;

  kernel_proc->threads = kernel_thread;
  kernel_proc->next = NULL; // FIX: Process list is NOT circular!
  process_list = kernel_proc;

  extern void scheduler_add_thread(thread_t * thread);
  scheduler_add_thread(kernel_thread);
  // Manual set for the very first one
  extern void scheduler_set_current_thread(thread_t * thread);
  scheduler_set_current_thread(kernel_thread);
}

process_t *process_create(const char *name, void (*entrypoint)(void)) {
  process_t *proc = (process_t *)kmalloc(sizeof(process_t));
  if (!proc)
    return NULL;
  memset(proc, 0, sizeof(process_t));
  proc->pid = next_pid++;
  proc->state = PROCESS_STATE_ALIVE;
  proc->exit_status = 0;
  
  process_t *parent = scheduler_get_current_process();
  proc->parent_pid = parent ? parent->pid : 0;
  
  kstrncpy(proc->name, name, 31);

  thread_t *thread = (thread_t *)kmalloc(sizeof(thread_t));
  if (!thread) {
    kfree(proc);
    return NULL;
  }
  memset(thread, 0, sizeof(thread_t));
  thread->tid = next_tid++;
  thread->state = THREAD_STATE_READY;
  thread->process = proc;
  proc->threads = thread;

  // Initialize FD table
  for (int i = 0; i < MAX_PROCESS_FDS; i++)
    proc->fd_table[i] = NULL;

  // Allocate kernel stack
  thread->kernel_stack = kmalloc(KERNEL_STACK_SIZE);
  if (!thread->kernel_stack) {
    kfree(thread);
    kfree(proc);
    return NULL;
  }
  memset(thread->kernel_stack, 0, KERNEL_STACK_SIZE);

  // Setup initial stack context
  uint32_t *stack =
      (uint32_t *)((uint32_t)thread->kernel_stack + KERNEL_STACK_SIZE);
  stack -= sizeof(registers_t) / 4;
  registers_t *regs = (registers_t *)stack;
  memset(regs, 0, sizeof(registers_t));

  regs->eflags = 0x202;
  regs->cs = 0x08;
  regs->ds = 0x10;
  regs->eip = (uint32_t)entrypoint;
  regs->ebp = (uint32_t)thread->kernel_stack + KERNEL_STACK_SIZE;
  regs->esp = (uint32_t)regs;

  thread->context = regs;
  proc->page_directory = (void *)0;

  proc->next = process_list;
  process_list = proc;

  extern void scheduler_add_thread(thread_t * thread);
  scheduler_add_thread(thread);

  kprintf("Process: Created '%s' (PID: %d, TID: %d) at 0x%x\n", name, proc->pid,
          thread->tid, (uint32_t)entrypoint);
  return proc;
}

process_t *process_create_user(const char *name, uint32_t entrypoint,
                               uint32_t user_stack_top) {
  process_t *proc = (process_t *)kmalloc(sizeof(process_t));
  if (!proc)
    return NULL;
  memset(proc, 0, sizeof(process_t));
  proc->pid = next_pid++;
  proc->state = PROCESS_STATE_ALIVE;
  proc->exit_status = 0;
  
  process_t *parent = scheduler_get_current_process();
  proc->parent_pid = parent ? parent->pid : 0;

  kstrncpy(proc->name, name, 31);

  thread_t *thread = (thread_t *)kmalloc(sizeof(thread_t));
  if (!thread) {
    kfree(proc);
    return NULL;
  }
  memset(thread, 0, sizeof(thread_t));
  thread->tid = next_tid++;
  thread->state = THREAD_STATE_READY;
  thread->process = proc;
  proc->threads = thread;

  for (int i = 0; i < MAX_PROCESS_FDS; i++)
    proc->fd_table[i] = NULL;

  thread->kernel_stack = kmalloc(KERNEL_STACK_SIZE);
  if (!thread->kernel_stack) {
    kfree(thread);
    kfree(proc);
    return NULL;
  }
  memset(thread->kernel_stack, 0, KERNEL_STACK_SIZE);

  uint32_t *stack =
      (uint32_t *)((uint32_t)thread->kernel_stack + KERNEL_STACK_SIZE);
  stack -= sizeof(registers_t) / 4;
  registers_t *regs = (registers_t *)stack;
  memset(regs, 0, sizeof(registers_t));

  regs->eflags = 0x202;
  regs->cs = 0x1B;
  regs->ds = 0x23;
  regs->ss = 0x23;
  regs->eip = entrypoint;
  regs->useresp = user_stack_top;
  regs->ebp = user_stack_top;

  thread->context = regs;
  proc->page_directory = vmm_clone_directory();

  proc->next = process_list;
  process_list = proc;
  // removed: scheduler_add_process(proc); Caller must add it manually after
  // loading PE.

  kprintf("Process: Created Userland '%s' (PID: %d, TID: %d) at 0x%x\n", name,
          proc->pid, thread->tid, entrypoint);
  return proc;
}

// Background Task Demo
void task_demo(void) {
  const char spinner[] = "|/-\\";
  int spin_idx = 0;

  while (1) {
    // Ekranın sağ üst kösesinde bir sayaç göster (Multitasking kanıtı)
    // gfx.h fonksiyonlarını doğrudan kernelden çağırabiliriz
    extern void gfx_puts(int x, int y, const char *str, uint32_t color);

    // Basit bir saniye/tick sayacı simülasyonu
    // Not: kprintf veya sprintf benzeri bir şey kullanarak formatlayabiliriz
    // Ama en basit haliyle sadece sabit yazı ve spinner basalım

    gfx_puts(850, 10, "BACKGROUND TASK:", 0xFFFFFF00); // Sarı

    char spin_str[2] = {spinner[spin_idx], '\0'};
    gfx_puts(1000, 10, spin_str, 0xFFFF0000); // Kırmızı spinner

    spin_idx = (spin_idx + 1) % 4;

    // Busy wait (Saniyede birkaç kez dönmesi için)
    for (volatile int i = 0; i < 2000000; i++)
      ;
  }
}

process_t *process_get_list(void) {
    return process_list;
}

process_t *process_get_by_pid(uint32_t pid) {
  process_t *curr = process_list;
  while (curr) {
    if (curr->pid == pid) return curr;
    curr = curr->next;
  }
  return NULL;
}

void process_terminate(process_t *proc, int status) {
  if (!proc || proc->state == PROCESS_STATE_ZOMBIE) return;
  proc->state = PROCESS_STATE_ZOMBIE;
  proc->exit_status = status;

  // Kill all threads
  thread_t *t = proc->threads;
  while (t) {
    t->state = THREAD_STATE_TERMINATED;
    t = t->next;
  }

  // Close all FDs
  for (int i = 0; i < MAX_PROCESS_FDS; i++) {
    if (proc->fd_table[i]) {
      vfs_close(proc->fd_table[i]);
      proc->fd_table[i] = NULL;
    }
  }

  kprintf("Process: PID %d (%s) terminated with status %d\n", proc->pid, proc->name, status);
}

void process_cleanup(process_t *proc) {
  if (!proc) return;

  // Remove from list
  if (process_list == proc) {
    process_list = proc->next;
  } else {
    process_t *curr = process_list;
    while (curr && curr->next != proc) {
      curr = curr->next;
    }
    if (curr) curr->next = proc->next;
  }

  // Free threads
  thread_t *t = proc->threads;
  while (t) {
    thread_t *next = t->next;
    if (t->kernel_stack) kfree(t->kernel_stack);
    kfree(t);
    t = next;
  }

  if (proc->page_directory) {
    vmm_free_directory(proc->page_directory);
  }

  kfree(proc);
}

int process_waitpid(uint32_t pid, int *status) {
  while (1) {
    process_t *child = process_get_by_pid(pid);
    if (!child) return -1; // Not found

    if (child->state == PROCESS_STATE_ZOMBIE) {
      if (status) *status = child->exit_status;
      process_cleanup(child);
      return pid;
    }
    // Simple busy-wait with yield simulation for now (since we don't have block states yet)
    // Actually, we can just yield context
    extern registers_t *scheduler_schedule(registers_t *regs);
    // Well we can't directly call it like this.
    // Just a basic delay loop works for now:
    for (volatile int i=0; i<100000; i++);
  }
}
