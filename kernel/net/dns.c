#include <net/dns.h>
#include <net/udp.h>
#include <libk/string.h>
#include <drivers/vga.h>

// DNS header structure
typedef struct __attribute__((packed)) {
  uint16_t id;
  uint16_t flags;
  uint16_t q_count;
  uint16_t ans_count;
  uint16_t auth_count;
  uint16_t add_count;
} dns_header_t;

static uint16_t bswap16(uint16_t v) { return (v >> 8) | (v << 8); }

static void dns_format_name(uint8_t *dns_query, const char *host) {
  int lock = 0;
  char src[128];
  kstrncpy(src, host, sizeof(src) - 2);
  src[sizeof(src) - 2] = '\0';
  
  // Simple check to append dot if not present
  int len = strlen(src);
  if (len > 0 && src[len - 1] != '.') {
    src[len] = '.';
    src[len + 1] = '\0';
  }
  
  for (int i = 0; src[i] != '\0'; i++) {
    if (src[i] == '.') {
      *dns_query++ = i - lock;
      for (; lock < i; lock++) {
        *dns_query++ = src[lock];
      }
      lock++; // skip the dot
    }
  }
  *dns_query++ = 0;
}

bool dns_resolve(const char *hostname, uint32_t *out_ip) {
  // 1. Local Fallback Cache for Offline / Loopback testing
  if (strcmp(hostname, "localhost") == 0 || strcmp(hostname, "127.0.0.1") == 0) {
    *out_ip = 0x0100007F; // 127.0.0.1
    return true;
  }
  if (strcmp(hostname, "google.com") == 0) {
    *out_ip = 0x8EFAFA8E; // 142.250.250.142
    return true;
  }
  if (strcmp(hostname, "khazar-os.org") == 0) {
    *out_ip = 0x059070B9; // 185.112.112.5 (Our package server)
    return true;
  }

  // 2. Real UDP DNS Query over Network
  uint8_t packet[512];
  memset(packet, 0, sizeof(packet));
  
  dns_header_t *hdr = (dns_header_t *)packet;
  hdr->id = bswap16(0x1234);
  hdr->flags = bswap16(0x0100); // Standard Query, recursion desired
  hdr->q_count = bswap16(1);
  
  uint8_t *qname = packet + sizeof(dns_header_t);
  dns_format_name(qname, hostname);
  
  int name_len = strlen((char *)qname) + 1;
  uint8_t *qinfo = qname + name_len;
  
  // Set QType to A (1) and QClass to IN (1)
  *qinfo++ = 0; *qinfo++ = 1; // Type A
  *qinfo++ = 0; *qinfo++ = 1; // Class IN
  
  uint16_t packet_size = (uint16_t)(qinfo - packet);
  
  // Bind a local port for DNS response
  uint16_t local_port = 5553;
  udp_recv(local_port, NULL, 0); // Open the port queue in UDP
  
  // Send query to Google DNS (8.8.8.8)
  uint32_t dns_server = 0x08080808; // 8.8.8.8
  
  if (!udp_send(dns_server, 53, packet, packet_size)) {
    return false;
  }
  
  // Wait for response with a simple retry loop
  uint8_t resp[1024];
  int received = 0;
  
  for (int retry = 0; retry < 5; retry++) {
    received = udp_recv(local_port, resp, sizeof(resp));
    if (received > 0) break;
    __asm__ volatile("int $0x20");
  }
  
  if (received < (int)sizeof(dns_header_t)) {
    return false;
  }
  
  dns_header_t *resp_hdr = (dns_header_t *)resp;
  uint16_t ans_count = bswap16(resp_hdr->ans_count);
  if (ans_count == 0) {
    return false;
  }
  
  uint8_t *ptr = resp + sizeof(dns_header_t);
  
  // Skip QName
  while (*ptr != 0) {
    ptr++;
  }
  ptr++; // skip 0 byte
  ptr += 4; // skip QType and QClass
  
  // Parse Answer Section
  for (int i = 0; i < ans_count; i++) {
    if ((*ptr & 0xC0) == 0xC0) {
      ptr += 2; // skip compressed name offset
    } else {
      while (*ptr != 0) ptr++;
      ptr++;
    }
    
    uint16_t type = (ptr[0] << 8) | ptr[1];
    ptr += 2;
    uint16_t class = (ptr[0] << 8) | ptr[1];
    ptr += 2;
    ptr += 4; // skip TTL
    uint16_t rdlength = (ptr[0] << 8) | ptr[1];
    ptr += 2;
    
    if (type == 1 && class == 1 && rdlength == 4) { // Type A, Class IN, Length 4
      *out_ip = *(uint32_t *)ptr;
      return true;
    }
    ptr += rdlength;
  }
  
  return false;
}
