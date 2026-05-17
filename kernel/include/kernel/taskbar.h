#ifndef TASKBAR_H
#define TASKBAR_H

#include <libk/types.h>

#define TASKBAR_HEIGHT 40
#define TASKBAR_COLOR 0x002B2B2B
#define TASKBAR_START_BTN_WIDTH 80

void taskbar_init(void);
void taskbar_draw(void);
void taskbar_update(void);

#endif
