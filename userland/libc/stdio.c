#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>


int puts(const char *s) {
  int len = strlen(s);
  return syscall3(SYS_WRITE, 1, (uint32_t)s, len); // fd 1 is stdout
}

static void print_uint(uint32_t value, int base) {
  char buffer[32];
  int i = 0;

  if (value == 0) {
    puts("0");
    return;
  }

  while (value > 0) {
    int r = value % base;
    if (r < 10)
      buffer[i++] = '0' + r;
    else
      buffer[i++] = 'a' + (r - 10);
    value /= base;
  }

  // Reverse and print
  char str[2] = {0};
  while (i > 0) {
    str[0] = buffer[--i];
    puts(str);
  }
}

static void print_int(int value) {
  if (value < 0) {
    puts("-");
    value = -value;
  }
  print_uint(value, 10);
}

int printf(const char *format, ...) {
  va_list args;
  va_start(args, format);

  for (int i = 0; format[i] != '\0'; i++) {
    if (format[i] == '%') {
      i++;
      switch (format[i]) {
      case 'd':
        print_int(va_arg(args, int));
        break;
      case 'x':
        print_uint(va_arg(args, uint32_t), 16);
        break;
      case 's':
        puts(va_arg(args, char *));
        break;
      case 'c': {
        char c_str[2] = {(char)va_arg(args, int), '\0'};
        puts(c_str);
        break;
      }
      case '%':
        puts("%");
        break;
      }
    } else {
      char c_str[2] = {format[i], '\0'};
      puts(c_str);
    }
  }

  va_end(args);
  return 0; // Simplified return
}

int fopen(const char *name) { return syscall1(SYS_OPEN, (uint32_t)name); }

int fclose(int fd) { return syscall1(SYS_CLOSE, (uint32_t)fd); }

int fread(int fd, void *buffer, uint32_t size) {
  return syscall3(SYS_READ, (uint32_t)fd, (uint32_t)buffer, size);
}

int fwrite(int fd, const void *buffer, uint32_t size) {
  return syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)buffer, size);
}
