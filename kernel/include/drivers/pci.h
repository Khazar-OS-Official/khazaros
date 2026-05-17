#ifndef PCI_H
#define PCI_H

#include <libk/types.h>

typedef struct {
  uint8_t bus;
  uint8_t dev;
  uint8_t func;
  uint16_t vendor_id;
  uint16_t device_id;
  uint8_t class_code;
  uint8_t subclass;
  uint8_t prog_if;
  uint8_t revision;
} pci_device_t;

// PCI cihazlarını scan edir və daxili siyahını doldurur
void pci_init(void);

uint32_t pci_get_device_count(void);
const pci_device_t *pci_get_device(uint32_t idx);

// Class/subclass-a görə ilk cihazı tap (tapılmazsa NULL)
const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass);
const pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

void pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);
void pci_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val);

#endif // PCI_H

