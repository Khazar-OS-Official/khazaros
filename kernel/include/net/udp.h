#ifndef UDP_H
#define UDP_H

#include <libk/types.h>

void udp_init(void);

// Send a UDP packet to dst_ip:dst_port
bool udp_send(uint32_t dst_ip, uint16_t dst_port, const void *payload,
              uint16_t len);

// Handle an incoming UDP packet (payload starts at UDP header)
void udp_handle(uint32_t src_ip, const uint8_t *segment, uint16_t len);

// Read a UDP payload from a specific local port into buf.
// Returns number of bytes, 0 if nothing received for that port.
int udp_recv(uint16_t local_port, uint8_t *buf, uint16_t maxlen);

#endif // UDP_H
