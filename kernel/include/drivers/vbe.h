#ifndef VBE_H
#define VBE_H

#include <kernel/multiboot.h>
#include <libk/types.h>

// VBE Sürücü Bilgileri
struct vbe_info {
  uint32_t *lfb; // Linear Framebuffer adresi
  uint32_t width;
  uint32_t height;
  uint32_t pitch; // Satır boyutu (bytes)
  uint8_t bpp;
};

void vbe_init(struct multiboot_info *mbi, uint32_t magic);
struct vbe_info *vbe_get_info(void);

#endif
