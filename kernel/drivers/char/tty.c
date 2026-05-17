#include <kernel/tty.h>
#include <libk/string.h>
#include <drivers/vga.h>

static struct tty main_tty;

void tty_init(void) {
  main_tty.head = 0;
  main_tty.tail = 0;
  main_tty.count = 0;
  memset(main_tty.buffer, 0, TTY_BUFFER_SIZE);
}

void tty_putc(char c) {
  if (main_tty.count < TTY_BUFFER_SIZE) {
    main_tty.buffer[main_tty.head] = c;
    main_tty.head = (main_tty.head + 1) % TTY_BUFFER_SIZE;
    main_tty.count++;
  }
}

char tty_getc(void) {
  if (main_tty.count == 0)
    return 0;

  char c = main_tty.buffer[main_tty.tail];
  main_tty.tail = (main_tty.tail + 1) % TTY_BUFFER_SIZE;
  main_tty.count--;
  return c;
}

uint32_t tty_read(char *buf, uint32_t size) {
  uint32_t read = 0;
  while (read < size) {
    char c = tty_getc();
    buf[read++] = c;
    if (c == '\n')
      break; // Satır bazlı okuma
  }
  return read;
}
