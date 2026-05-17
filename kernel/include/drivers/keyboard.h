#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <libk/types.h>

// Keyboard ports
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

// PIC data port (IRQ masking)
#define PIC1_DATA 0x21

// Special key scancodes
#define KEY_UP_ARROW    0x48
#define KEY_DOWN_ARROW  0x50
#define KEY_LEFT_ARROW  0x4B
#define KEY_RIGHT_ARROW 0x4D
#define KEY_TAB         0x0F
#define KEY_HOME        0x47
#define KEY_END         0x4F
#define KEY_DELETE      0x53

// Initialize keyboard driver
void keyboard_init(void);

// Returns the last special scancode (UP/DOWN/TAB etc.) and clears it.
// Returns 0 if no special key was pressed since last call.
uint8_t keyboard_get_special(void);

#endif // KEYBOARD_H
