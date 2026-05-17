#ifndef WAIT_H
#define WAIT_H

#include <proc/process.h>

// wait_queue_t: A simple mechanism to block/unblock processes
// inspired by the reference kernel's WaitQueue.
typedef struct {
  thread_t *threads[MAX_PROCESSES];
  int count;
} wait_queue_t;

void wait_queue_init(wait_queue_t *q);
void wait_on(wait_queue_t *q);
void wake_up(wait_queue_t *q);

#endif // WAIT_H
