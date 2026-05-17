#ifndef IPV4_H
#define IPV4_H

#include <libk/types.h>

// Our static IP configuration - stored in LITTLE-ENDIAN (host byte order).
// x86 is little-endian: when uint32 is copied into packet bytes it comes out
// in network (big-endian) order as required by the wire format.
// Formula: a|(b<<8)|(c<<16)|(d<<24) for IP a.b.c.d
#define NET_OUR_IP 0x0F02000A     // 10.0.2.15
#define NET_GATEWAY_IP 0x0202000A // 10.0.2.2
#define NET_NETMASK 0x00FFFFFF    // 255.255.255.0
#define NET_BCAST_IP 0xFF02000A   // 10.0.2.255

// IP protocol numbers
#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP 17

void ipv4_init(void);

// Send an IP packet (protocol = IP_PROTO_*)
bool ipv4_send(uint32_t dst_ip, uint8_t protocol, const void *payload,
               uint16_t len);

// Handle an incoming raw Ethernet payload that has EtherType=0x0800
void ipv4_handle(const uint8_t *frame, uint16_t frame_len);

#endif // IPV4_H
