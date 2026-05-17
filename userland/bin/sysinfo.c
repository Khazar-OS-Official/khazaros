#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  printf("========================================\n");
  printf("          Khazar OS Sysinfo             \n");
  printf("========================================\n");
  printf("OS Version: Khazar OS v0.2.0\n");
  printf("Architecture: x86 (32-bit Protected Mode)\n");
  printf("Kernel: Monolithic, Custom\n");
  printf("Filesystem: FAT32 over ATA/LBA28 PIO\n");
  printf("GUI: VBE VESA Framebuffer @ 1024x768x32\n");
  printf("Network: Intel E1000 (ARP/IPv4/UDP)\n");
  printf("Userland: Custom libc, PE binaries\n");
  printf("========================================\n");
  
  return 0;
}
