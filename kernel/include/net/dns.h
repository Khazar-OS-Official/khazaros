#ifndef DNS_H
#define DNS_H

#include <libk/types.h>

// Resolve domain name to IP address. Returns true on success.
bool dns_resolve(const char *hostname, uint32_t *out_ip);

#endif // DNS_H
