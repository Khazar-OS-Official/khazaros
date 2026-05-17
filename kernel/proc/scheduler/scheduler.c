#include <arch/gdt.h>
#include <drivers/vga.h>
#include <mm/vmm.h>
#include <proc/process.h>

static thread_t *current_thread = NULL;
static thread_t *ready_queue = NULL;

void scheduler_set_current_thread(thread_t *thread) {
  current_thread = thread;
  if (!ready_queue)
    ready_queue = thread;
}

void scheduler_init(void) {
  kprintf("Scheduler: Initializing Round-Robin Scheduler...\n");
}

thread_t *scheduler_get_current_thread(void) { return current_thread; }

process_t *scheduler_get_current_process(void) {
  return current_thread ? current_thread->process : NULL;
}

void scheduler_add_thread(thread_t *thread) {
  if (!ready_queue) {
    ready_queue = thread;
    thread->next = thread; // Circular list
  } else {
    thread_t *last = ready_queue;
    while (last->next != ready_queue) {
      last = last->next;
    }
    last->next = thread;
    thread->next = ready_queue;
  }
}

// This is called from the timer interrupt or yield
registers_t *scheduler_schedule(registers_t *regs) {
  if (!ready_queue)
    return regs;

  // Save context of current thread
  if (current_thread) {
    current_thread->context = regs;
  }

  // Periodic Zombie Process Cleanup
  static int sched_ticks = 0;
  if (++sched_ticks > 100) { // Check every ~100 ticks to avoid overhead
    sched_ticks = 0;
    process_t *curr = process_get_list();
    while (curr) {
      process_t *next = curr->next;
      if (curr->state == PROCESS_STATE_ZOMBIE) {
        process_t *parent = process_get_by_pid(curr->parent_pid);
        // If parent doesn't exist, or parent is itself a zombie, reap the child
        if (!parent || parent->state == PROCESS_STATE_ZOMBIE) {
          process_cleanup(curr);
        }
      }
      curr = next;
    }
  }

  // Pick next thread (Round-Robin)
  if (!current_thread) {
    current_thread = ready_queue;
  } else {
    current_thread = current_thread->next;
  }

  // Count threads to prevent infinite loop
  int queue_size = 0;
  thread_t *temp = ready_queue;
  if (temp) {
    do {
      queue_size++;
      temp = temp->next;
    } while (temp != ready_queue);
  }

  // Ensure we don't pick a blocked or terminated thread
  int checked = 0;
  while (current_thread && 
         current_thread->state != THREAD_STATE_READY &&
         current_thread->state != THREAD_STATE_RUNNING &&
         checked < queue_size) {
    current_thread = current_thread->next;
    checked++;
  }

  // If no runnable thread found, return original context
  if (!current_thread || current_thread->state == THREAD_STATE_TERMINATED) {
    current_thread = NULL;

    // Switch to kernel directory just to be safe
    vmm_switch_directory(
        (struct page_directory *)V2P(vmm_get_kernel_directory()));

    return regs;
  }

  current_thread->state = THREAD_STATE_RUNNING;

  // Update TSS ESP0 to point to the current thread's kernel stack
  if (current_thread->kernel_stack) {
    set_kernel_stack((uint32_t)current_thread->kernel_stack +
                     KERNEL_STACK_SIZE);
  }

  if (!current_thread->context) {
    return regs;
  }

  // Use the process's page directory
  process_t *proc = current_thread->process;
  if (proc && proc->page_directory) {
    vmm_switch_directory((struct page_directory *)V2P(proc->page_directory));
  } else {
    vmm_switch_directory(
        (struct page_directory *)V2P(vmm_get_kernel_directory()));
  }

  // Resume context
  return current_thread->context;
}
