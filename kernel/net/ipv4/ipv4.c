// IPv4 - Internet Protocol version 4
// Minimal implementation: static IP config, send/receive
#include <drivers/vga.h>
#include <libk/string.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>

// IPv4 header
typedef struct __attribute__((packed)) {
  uint8_t ver_ihl; // Version (4) + IHL (5 = 20 bytes, no opts)
  uint8_t tos;
  uint16_t total_len; // Big-endian total length
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint32_t src_ip;
  uint32_t dst_ip;
} ipv4_header_t;

static uint16_t ip_id = 0;

// 16-bit one's complement checksum (RFC 791)
static uint16_t ip_checksum(const void *data, uint16_t len) {
  const uint16_t *ptr = (const uint16_t *)data;
  uint32_t sum = 0;
  while (len > 1) {
    sum += *ptr++;
    len -= 2;
  }
  if (len)
    sum += *(uint8_t *)ptr;
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  return (uint16_t)~sum;
}

// Byte-swap 16-bit (host<->network)
static uint16_t bswap16(uint16_t v) { return (v >> 8) | (v << 8); }

void ipv4_init(void) { ip_id = 0; }

bool ipv4_send(uint32_t dst_ip, uint8_t protocol, const void *payload,
               uint16_t plen) {
  // For VirtualBox NAT, always ARP for dst_ip directly.
  // (The gateway 10.0.2.2 is reachable by direct ARP on the virtual LAN.)
  uint8_t dst_mac[6];
  if (!arp_lookup(dst_ip, dst_mac)) {
    arp_request(dst_ip);
    kprintf("IPv4: ARP miss for target, sending request...\n");
    return false;
  }

  uint16_t total = sizeof(ipv4_header_t) + plen;
  uint8_t buf[1500];
  if (total > sizeof(buf))
    return false;

  // Build IPv4 header
  ipv4_header_t *hdr = (ipv4_header_t *)buf;
  hdr->ver_ihl = 0x45;
  hdr->tos = 0;
  hdr->total_len = bswap16(total);
  hdr->id = bswap16(ip_id++);
  hdr->frag_off = 0;
  hdr->ttl = 64;
  hdr->protocol = protocol;
  hdr->checksum = 0;
  hdr->src_ip = NET_OUR_IP;
  hdr->dst_ip = dst_ip;
  hdr->checksum = ip_checksum(hdr, sizeof(ipv4_header_t));

  memcpy(buf + sizeof(ipv4_header_t), payload, plen);
  return ethernet_send(dst_mac, ETHERTYPE_IP, buf, total);
}

void ipv4_handle(const uint8_t *frame, uint16_t frame_len) {
  if (frame_len < 14 + sizeof(ipv4_header_t))
    return;

  // Skip Ethernet header (14 bytes)
  const ipv4_header_t *hdr = (const ipv4_header_t *)(frame + 14);
  uint8_t ihl = (hdr->ver_ihl & 0x0F) * 4;
  uint16_t total = bswap16(hdr->total_len);
  const uint8_t *payload = (const uint8_t *)hdr + ihl;
  uint16_t plen = total - ihl;

  // Cache sender in ARP table
  uint8_t src_mac[6];
  memcpy(src_mac, frame + 6, 6); // src MAC is at frame[6..11]
  extern void arp_cache_put(uint32_t ip, const uint8_t *mac);
  // Note: arp_cache_put is static inside arp.c
  // We go through arp_handle as a fake "ARP reply" won't work,
  // so we rely only on actual ARP traffic for cache filling.

  if (hdr->protocol == IP_PROTO_UDP) {
    udp_handle(hdr->src_ip, payload, plen);
  }
}
