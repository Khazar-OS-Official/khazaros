#ifndef TTY_H
#define TTY_H

#include <libk/types.h>

#define TTY_BUFFER_SIZE 1024

// TTY yapısı - Basit dairesel buffer
struct tty {
  char buffer[TTY_BUFFER_SIZE];
  uint32_t head;
  uint32_t tail;
  uint32_t count;
};

// Fonksiyonlar
void tty_init(void);
void tty_putc(char c); // Klavyeden karakter ekle
char tty_getc(void);   // Buffer'dan karakter oku (bloklayan)
uint32_t tty_read(char *buf, uint32_t size); // Birden fazla karakter oku

#endif // TTY_H
