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

// Dynamic IP Configuration variables
uint32_t net_our_ip     = 0x0F02000A; // 10.0.2.15 (Default Little Endian)
uint32_t net_gateway_ip = 0x0202000A; // 10.0.2.2
uint32_t net_netmask    = 0x00FFFFFF; // 255.255.255.0
uint32_t net_bcast_ip   = 0xFF02000A; // 10.0.2.255

// ICMP header
typedef struct __attribute__((packed)) {
  uint8_t type;
  uint8_t code;
  uint16_t checksum;
  uint16_t id;
  uint16_t seq;
} icmp_header_t;

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
  // Loopback (127.0.0.1 -> 0x0100007F) or sending to our own IP
  if (dst_ip == 0x0100007F || dst_ip == net_our_ip) {
    uint8_t loop_buf[1518];
    uint16_t total = sizeof(ipv4_header_t) + plen;
    if (total + 14 > sizeof(loop_buf)) return false;

    // Mock Ethernet header (14 bytes)
    memset(loop_buf, 0, 14);
    loop_buf[12] = 0x08; // IPv4 (0x0800 big-endian)
    loop_buf[13] = 0x00;

    ipv4_header_t *hdr = (ipv4_header_t *)(loop_buf + 14);
    hdr->ver_ihl = 0x45;
    hdr->tos = 0;
    hdr->total_len = bswap16(total);
    hdr->id = bswap16(ip_id++);
    hdr->frag_off = 0;
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    hdr->src_ip = net_our_ip;
    hdr->dst_ip = dst_ip;
    hdr->checksum = ip_checksum(hdr, sizeof(ipv4_header_t));

    memcpy(loop_buf + 14 + sizeof(ipv4_header_t), payload, plen);

    // Bypasses the network card, loopback processed immediately in receiving path
    ipv4_handle(loop_buf, total + 14);
    return true;
  }

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
  hdr->src_ip = net_our_ip;
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

  if (hdr->protocol == IP_PROTO_UDP) {
    udp_handle(hdr->src_ip, payload, plen);
  } else if (hdr->protocol == IP_PROTO_ICMP) {
    if (plen >= sizeof(icmp_header_t)) {
      icmp_header_t *icmp = (icmp_header_t *)payload;
      if (icmp->type == 8) { // Echo Request
        kprintf("ICMP: Echo Request from %d.%d.%d.%d\n",
                (hdr->src_ip >> 0) & 0xFF, (hdr->src_ip >> 8) & 0xFF,
                (hdr->src_ip >> 16) & 0xFF, (hdr->src_ip >> 24) & 0xFF);
                
        uint8_t rep_buf[512];
        if (plen > sizeof(rep_buf)) plen = sizeof(rep_buf);
        memcpy(rep_buf, payload, plen);
        
        icmp_header_t *rep_icmp = (icmp_header_t *)rep_buf;
        rep_icmp->type = 0; // Echo Reply
        rep_icmp->code = 0;
        rep_icmp->checksum = 0;
        rep_icmp->checksum = ip_checksum(rep_icmp, plen);
        
        ipv4_send(hdr->src_ip, IP_PROTO_ICMP, rep_buf, plen);
      } else if (icmp->type == 0) { // Echo Reply
        kprintf("ICMP: Echo Reply from %d.%d.%d.%d (seq=%d)\n",
                (hdr->src_ip >> 0) & 0xFF, (hdr->src_ip >> 8) & 0xFF,
                (hdr->src_ip >> 16) & 0xFF, (hdr->src_ip >> 24) & 0xFF,
                bswap16(icmp->seq));
      }
    }
  }
}

bool icmp_send_request(uint32_t dst_ip) {
  icmp_header_t pkt;
  pkt.type = 8; // Echo Request
  pkt.code = 0;
  pkt.checksum = 0;
  pkt.id = bswap16(1234);
  pkt.seq = bswap16(1);
  pkt.checksum = ip_checksum(&pkt, sizeof(pkt));
  
  kprintf("ICMP: Sending Echo Request to %d.%d.%d.%d...\n",
          (dst_ip >> 0) & 0xFF, (dst_ip >> 8) & 0xFF,
          (dst_ip >> 16) & 0xFF, (dst_ip >> 24) & 0xFF);
          
  return ipv4_send(dst_ip, IP_PROTO_ICMP, &pkt, sizeof(pkt));
}
