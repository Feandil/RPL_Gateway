#include "lib/tool.h"

#include <string.h>
#include <stdio.h>

typedef struct ipv6_addr {
  uint32_t ip[4];
} ipv6_addr;

int equal(struct in6_addr* a, uip_ip6addr_t* b)
{
  return memcmp(a,b,sizeof(uip_ip6addr_t)) == 0;
}

void toString(uip_ipaddr_t* b, char* s)
{
  uint16_t a;
  unsigned int i;
  int f;
  int ret;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (b->u8[i] << 8) + b->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        ret = sprintf(s,"%s","::");
        if(ret > 0) {
          s+=ret;
        }
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        ret = sprintf(s,"%s",":");
        if(ret > 0) {
          s+=ret;
        }
      }
        ret = sprintf(s,"%x",a);
        if(ret > 0) {
          s+=ret;
        }
    }
  }
}
