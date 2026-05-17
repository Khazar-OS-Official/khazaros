#ifndef IPV4_H
#define IPV4_H

#include <libk/types.h>

// Dynamic network configuration
extern uint32_t net_our_ip;
extern uint32_t net_gateway_ip;
extern uint32_t net_netmask;
extern uint32_t net_bcast_ip;

#define NET_OUR_IP      net_our_ip
#define NET_GATEWAY_IP  net_gateway_ip
#define NET_NETMASK     net_netmask
#define NET_BCAST_IP    net_bcast_ip

// IP protocol numbers
#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP 17

void ipv4_init(void);

// Send an IP packet (protocol = IP_PROTO_*)
bool ipv4_send(uint32_t dst_ip, uint8_t protocol, const void *payload,
               uint16_t len);

// Handle an incoming raw Ethernet payload that has EtherType=0x0800
void ipv4_handle(const uint8_t *frame, uint16_t frame_len);

// Send an ICMP Echo Request (Ping)
bool icmp_send_request(uint32_t dst_ip);

#endif // IPV4_H
