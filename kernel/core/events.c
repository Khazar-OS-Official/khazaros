#include <kernel/events.h>
#include <libk/string.h>

#define EVENT_QUEUE_SIZE 128

static event_t queue[EVENT_QUEUE_SIZE];
static int head = 0;
static int tail = 0;

void events_init(void) {
    head = 0;
    tail = 0;
    memset(queue, 0, sizeof(queue));
}

void events_push(event_t event) {
    int next = (tail + 1) % EVENT_QUEUE_SIZE;
    if (next == head) return; // Full
    queue[tail] = event;
    tail = next;
}

bool events_pop(event_t *event) {
    if (head == tail) return false; // Empty
    *event = queue[head];
    head = (head + 1) % EVENT_QUEUE_SIZE;
    return true;
}
