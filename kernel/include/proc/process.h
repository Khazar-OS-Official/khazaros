#ifndef PROCESS_H
#define PROCESS_H

#include <arch/isr.h>
#include <fs/vfs.h>
#include <libk/types.h>

#define MAX_PROCESSES 64
#define MAX_PROCESS_FDS 16
#define KERNEL_STACK_SIZE 8192

typedef enum {
  THREAD_STATE_READY,
  THREAD_STATE_RUNNING,
  THREAD_STATE_BLOCKED,
  THREAD_STATE_TERMINATED
} thread_state_t;

struct process; // Forward declaration

typedef struct thread {
  uint32_t tid;
  thread_state_t state;
  registers_t *context; // ESP pointer to saved registers on stack
  void *kernel_stack;   // Pointer to kernel stack bottom
  struct process *process; // Parent process

  struct thread *next; // Next in ready queue (scheduler)
} thread_t;

typedef enum {
  PROCESS_STATE_ALIVE,
  PROCESS_STATE_ZOMBIE
} process_state_t;

typedef struct process {
  uint32_t pid;
  uint32_t parent_pid;
  process_state_t state;
  int exit_status;

  char name[32];
  char cmdline[128];
  void *page_directory; // Physical address
  vfs_node_t *fd_table[MAX_PROCESS_FDS]; // File descriptor table

  thread_t *threads; // List of threads in this process
  struct process *next; // Next in process list
} process_t;

void process_init(void);
void process_terminate(process_t *proc, int status);
void process_cleanup(process_t *proc);
int process_waitpid(uint32_t pid, int *status);
process_t *process_get_by_pid(uint32_t pid);

void process_init(void);
process_t *process_create(const char *name, void (*entrypoint)(void));
process_t *process_create_user(const char *name, uint32_t entry_point,
                               uint32_t user_stack_top);
void enter_user_mode(uint32_t entry_point, uint32_t user_stack);

void scheduler_init(void);
thread_t *scheduler_get_current_thread(void);
void scheduler_add_thread(thread_t *thread);
registers_t *scheduler_schedule(registers_t *regs);

process_t *process_get_list(void);

// Helper for backward compatibility or process-level access
process_t *scheduler_get_current_process(void);

#endif // PROCESS_H
