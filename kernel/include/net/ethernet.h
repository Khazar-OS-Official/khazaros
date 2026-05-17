#ifndef ETHERNET_H
#define ETHERNET_H

#include <drivers/pci.h>
#include <libk/types.h>


#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP 0x0800

typedef struct {
  const pci_device_t *pci_dev;
  uint32_t mmio_base;
  uint8_t mac[6];
  bool initialized;
} ethernet_device_t;

bool ethernet_init(void);
bool ethernet_is_ready(void);
const ethernet_device_t *ethernet_get_device(void);

// Send an Ethernet frame (dst_mac=6 bytes, ethertype=big-endian, payload)
bool ethernet_send(const uint8_t *dst_mac, uint16_t ethertype,
                   const void *payload, uint16_t len);

// Poll for received frame. Returns bytes received, 0=none, -1=error
int ethernet_receive(uint8_t *buf, uint16_t maxlen);

#endif // ETHERNET_H
