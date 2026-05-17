#ifndef SERIAL_H
#define SERIAL_H

#include <libk/types.h>

// Serial COM1 port (0x3F8) – real PC debug üçün vacibdir
void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *str);

// kprintf wrapper (VGA terminal + serial eyni vaxtda)
void serial_kprintf(const char *fmt, ...);

#endif // SERIAL_H
