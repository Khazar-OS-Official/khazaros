#ifndef EVENTS_H
#define EVENTS_H

#include <libk/types.h>

typedef enum {
    EVENT_NONE,
    EVENT_KEY_DOWN,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_CLICK,
    EVENT_MOUSE_RELEASE
} event_type_t;

typedef struct {
    event_type_t type;
    int data1;
    int data2;
    int data3;
} event_t;

void events_init(void);
void events_push(event_t event);
bool events_pop(event_t *event);

#endif // EVENTS_H
