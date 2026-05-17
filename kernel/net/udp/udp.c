// UDP - User Datagram Protocol
// Multi-queue implementation for Khazar OS Phase 8
#include <drivers/vga.h>
#include <libk/string.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <proc/wait.h>

#define MAX_UDP_PORTS 16
#define UDP_BUF_SIZE 2048

typedef struct {
  uint16_t port;
  uint8_t buffer[UDP_BUF_SIZE];
  uint16_t length;
  bool active;
  wait_queue_t wait_queue;
} udp_queue_t;

static udp_queue_t udp_queues[MAX_UDP_PORTS];

static uint16_t bswap16(uint16_t v) { return (v >> 8) | (v << 8); }

// UDP header
typedef struct __attribute__((packed)) {
  uint16_t src_port;
  uint16_t dst_port;
  uint16_t length;
  uint16_t checksum;
} udp_header_t;

void udp_init(void) {
  for (int i = 0; i < MAX_UDP_PORTS; i++) {
    udp_queues[i].active = false;
    udp_queues[i].length = 0;
    wait_queue_init(&udp_queues[i].wait_queue);
  }
}

bool udp_send(uint32_t dst_ip, uint16_t dst_port, const void *payload,
              uint16_t plen) {
  // Use a default src port for raw sends if not bound (simplified)
  uint16_t src_port = 1234;

  uint16_t udp_total = sizeof(udp_header_t) + plen;
  uint8_t buf[1472];
  if (udp_total > sizeof(buf))
    return false;

  udp_header_t *hdr = (udp_header_t *)buf;
  hdr->src_port = bswap16(src_port);
  hdr->dst_port = bswap16(dst_port);
  hdr->length = bswap16(udp_total);
  hdr->checksum = 0;

  memcpy(buf + sizeof(udp_header_t), payload, plen);
  return ipv4_send(dst_ip, IP_PROTO_UDP, buf, udp_total);
}

void udp_handle(uint32_t src_ip, const uint8_t *segment, uint16_t len) {
  if (len < sizeof(udp_header_t))
    return;

  const udp_header_t *hdr = (const udp_header_t *)segment;
  uint16_t dst_port = bswap16(hdr->dst_port);
  uint16_t data_len = bswap16(hdr->length) - sizeof(udp_header_t);

  // Find queue for this port
  int slot = -1;
  for (int i = 0; i < MAX_UDP_PORTS; i++) {
    if (udp_queues[i].active && udp_queues[i].port == dst_port) {
      slot = i;
      break;
    }
  }

  // If no active queue, find an empty one to "auto-bind" (simplified)
  if (slot == -1) {
    for (int i = 0; i < MAX_UDP_PORTS; i++) {
      if (!udp_queues[i].active) {
        udp_queues[i].active = true;
        udp_queues[i].port = dst_port;
        slot = i;
        break;
      }
    }
  }

  if (slot != -1) {
    if (data_len > UDP_BUF_SIZE)
      data_len = UDP_BUF_SIZE;
    memcpy(udp_queues[slot].buffer, segment + sizeof(udp_header_t), data_len);
    udp_queues[slot].length = data_len;

    // Wake up any process waiting for data on this port
    wake_up(&udp_queues[slot].wait_queue);
  }
  (void)src_ip;
}

int udp_recv(uint16_t local_port, uint8_t *buf, uint16_t maxlen) {
  for (int i = 0; i < MAX_UDP_PORTS; i++) {
    if (udp_queues[i].active && udp_queues[i].port == local_port) {
      // If no data, block until wake_up is called by udp_handle
      while (udp_queues[i].length == 0) {
        wait_on(&udp_queues[i].wait_queue);
      }

      uint16_t n =
          (udp_queues[i].length < maxlen) ? udp_queues[i].length : maxlen;
      memcpy(buf, udp_queues[i].buffer, n);
      udp_queues[i].length = 0; // Consume
      return n;
    }
  }

  // If port not active yet, activate it for future packets
  for (int i = 0; i < MAX_UDP_PORTS; i++) {
    if (!udp_queues[i].active) {
      udp_queues[i].active = true;
      udp_queues[i].port = local_port;
      udp_queues[i].length = 0;
      return 0;
    }
  }

  return 0;
}
