// ARP - Address Resolution Protocol
// Resolves IPv4 addresses to Ethernet MAC addresses
#include <drivers/vga.h>
#include <libk/string.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>


#define ARP_CACHE_SIZE 8

// ARP packet structure
typedef struct __attribute__((packed)) {
  uint16_t htype; // Hardware type (1 = Ethernet)
  uint16_t ptype; // Protocol type (0x0800 = IPv4)
  uint8_t hlen;   // Hardware address length (6)
  uint8_t plen;   // Protocol address length (4)
  uint16_t oper;  // Operation: 1=request, 2=reply
  uint8_t sha[6]; // Sender hardware address
  uint32_t spa;   // Sender protocol address
  uint8_t tha[6]; // Target hardware address
  uint32_t tpa;   // Target protocol address
} arp_packet_t;

// Simple ARP cache
typedef struct {
  uint32_t ip;
  uint8_t mac[6];
  bool valid;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void arp_init(void) {
  for (int i = 0; i < ARP_CACHE_SIZE; i++)
    arp_cache[i].valid = false;
}

// Add or update an ARP cache entry
static void arp_cache_put(uint32_t ip, const uint8_t *mac) {
  // Check if already present
  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (arp_cache[i].valid && arp_cache[i].ip == ip) {
      memcpy(arp_cache[i].mac, mac, 6);
      return;
    }
  }
  // Find empty slot
  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (!arp_cache[i].valid) {
      arp_cache[i].ip = ip;
      arp_cache[i].valid = true;
      memcpy(arp_cache[i].mac, mac, 6);
      return;
    }
  }
  // Cache full - overwrite slot 0 (simple eviction)
  arp_cache[0].ip = ip;
  arp_cache[0].valid = true;
  memcpy(arp_cache[0].mac, mac, 6);
}

bool arp_lookup(uint32_t ip, uint8_t *dst_mac) {
  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (arp_cache[i].valid && arp_cache[i].ip == ip) {
      memcpy(dst_mac, arp_cache[i].mac, 6);
      return true;
    }
  }
  return false;
}

void arp_request(uint32_t target_ip) {
  const ethernet_device_t *eth = ethernet_get_device();
  if (!eth)
    return;

  arp_packet_t pkt;
  pkt.htype = 0x0100; // 1 = Ethernet (big-endian)
  pkt.ptype = 0x0008; // 0x0800 = IPv4 (big-endian)
  pkt.hlen = 6;
  pkt.plen = 4;
  pkt.oper = 0x0100; // 1 = request (big-endian)
  memcpy(pkt.sha, eth->mac, 6);
  pkt.spa = NET_OUR_IP;
  memset(pkt.tha, 0, 6);
  pkt.tpa = target_ip;

  ethernet_send(BROADCAST_MAC, ETHERTYPE_ARP, &pkt, sizeof(pkt));
  kprintf("ARP: Sent request for %d.%d.%d.%d\n", (target_ip >> 0) & 0xFF,
          (target_ip >> 8) & 0xFF, (target_ip >> 16) & 0xFF,
          (target_ip >> 24) & 0xFF);
}

void arp_handle(const uint8_t *payload, uint16_t len) {
  if (len < sizeof(arp_packet_t))
    return;
  const arp_packet_t *pkt = (const arp_packet_t *)payload;

  // Only handle IPv4 over Ethernet replies and requests
  if (pkt->htype != 0x0100 || pkt->ptype != 0x0008)
    return;

  // Cache the sender's entry
  arp_cache_put(pkt->spa, pkt->sha);

  // If this is an ARP request directed at us, send a reply
  if (pkt->oper == 0x0100 && pkt->tpa == NET_OUR_IP) {
    const ethernet_device_t *eth = ethernet_get_device();
    if (!eth)
      return;

    arp_packet_t reply;
    reply.htype = 0x0100;
    reply.ptype = 0x0008;
    reply.hlen = 6;
    reply.plen = 4;
    reply.oper = 0x0200; // 2 = reply
    memcpy(reply.sha, eth->mac, 6);
    reply.spa = NET_OUR_IP;
    memcpy(reply.tha, pkt->sha, 6);
    reply.tpa = pkt->spa;
    ethernet_send(pkt->sha, ETHERTYPE_ARP, &reply, sizeof(reply));
    kprintf("ARP: Sent reply to requester\n");
  }
}
