#include <stdlib.h>
#include <syscall.h>

extern int main(int argc, char **argv);

void start_c(void) {
  char cmdline[128];
  for(int i=0; i<128; i++) cmdline[i] = 0;
  // Ask kernel for the command line that launched us
  syscall2(SYS_GETCMDLINE, (uint32_t)cmdline, sizeof(cmdline));

  int argc = 0;
  char *argv[16];

  char *p = cmdline;
  while (*p) {
    // Skip leading spaces
    while (*p == ' ')
      p++;
    if (!*p)
      break;

    // Start of a token
    argv[argc++] = p;
    if (argc >= 16)
      break;

    // Fast-forward to end of token or string
    while (*p && *p != ' ')
      p++;

    // Null-terminate the token if it's not the end of the string
    if (*p) {
      *p = '\0';
      p++;
    }
  }

  // Call the standard C main function
  int ret = main(argc, argv);

  // Terminate execution safely
  exit(ret);
}
