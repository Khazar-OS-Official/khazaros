#include <proc/process.h>
#include <proc/wait.h>

void wait_queue_init(wait_queue_t *q) { q->count = 0; }

void wait_on(wait_queue_t *q) {
  thread_t *curr = scheduler_get_current_thread();
  if (!curr)
    return;

  if (q->count < MAX_PROCESSES) {
    curr->state = THREAD_STATE_BLOCKED;
    q->threads[q->count++] = curr;

    // Yield to the scheduler
    __asm__ volatile("int $0x20");
  }
}

void wake_up(wait_queue_t *q) {
  for (int i = 0; i < q->count; i++) {
    q->threads[i]->state = THREAD_STATE_READY;
  }
  q->count = 0;
}
