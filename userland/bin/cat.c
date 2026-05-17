#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: cat <filename>\n");
    return 1;
  }

  const char *filename = argv[1];
  int fd = fopen(filename);

  if (fd < 0) {
    printf("cat: %s: No such file or directory\n", filename);
    return 1;
  }

  char buffer[2048];
  int bytes_read = fread(fd, buffer, sizeof(buffer) - 1);
  if (bytes_read > 0) {
    buffer[bytes_read] = '\0';
    printf("%s", buffer);
  }

  fclose(fd);
  printf("\n");

  return 0;
}
