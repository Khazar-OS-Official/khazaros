#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("========================================\n");
  printf("          Khazar OS Sysinfo             \n");
  printf("========================================\n");
  printf("OS Version: Khazar OS v0.2.0\n");
  printf("Architecture: x86 (32-bit Protected Mode)\n");
  printf("Kernel: Monolithic, Custom\n");
  printf("Filesystem: FAT32 over ATA/LBA28 PIO\n");
  printf("GUI: VBE VESA Framebuffer @ 1024x768x32\n");
  
  // Read active network driver dynamically from /dev/netinfo
  int fd = fopen("/dev/netinfo");
  if (fd >= 3) {
    char netbuf[128];
    memset(netbuf, 0, sizeof(netbuf));
    int bytes = fread(fd, netbuf, sizeof(netbuf) - 1);
    if (bytes > 0) {
      printf("Network: %s\n", netbuf);
    } else {
      printf("Network: None\n");
    }
    fclose(fd);
  } else {
    printf("Network: None (Not active)\n");
  }

  printf("Userland: Custom libc, PE binaries\n");
  printf("========================================\n");
  
  return 0;
}
