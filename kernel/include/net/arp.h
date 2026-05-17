#ifndef ARP_H
#define ARP_H

#include <libk/types.h>

// Initialize ARP subsystem (clears cache)
void arp_init(void);

// Send an ARP request broadcast for the given IP
void arp_request(uint32_t target_ip);

// Handle an incoming ARP packet (raw Ethernet payload after EtherType)
void arp_handle(const uint8_t *payload, uint16_t len);

// Look up MAC for an IP in the ARP cache.
// Returns true and fills dst_mac if found, false otherwise.
bool arp_lookup(uint32_t ip, uint8_t *dst_mac);

#endif // ARP_H
