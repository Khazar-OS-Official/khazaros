#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      printf("%s", argv[i]);
      if (i < argc - 1) {
        printf(" ");
      }
    }
  } else {
    printf("Hello from Khazar OS Userland (Ring 3)!");
  }
  printf("\n");
  return 0;
}
