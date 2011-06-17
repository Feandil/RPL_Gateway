#ifndef TOOL_H
#define TOOL_H

#include "uip6.h"
#include <netinet/in.h>

int equal(struct in6_addr* a, uip_ipaddr_t* b);
void convert(struct in6_addr* a, uip_ipaddr_t* b);
void toString(uip_ipaddr_t* b, char* s);

#endif /* TOOL_H */

