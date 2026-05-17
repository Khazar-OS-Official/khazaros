#include <stdio.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  // Su an icin CWD destegi olmadigindan hep root donuyoruz.
  printf("/\n");
  return 0;
}
