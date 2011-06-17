#include "lib/tool.h"

#include <string.h>
#include <stdio.h>

typedef struct ipv6_addr {
  uint32_t ip[4];
} ipv6_addr;

int equal(struct in6_addr* a, uip_ip6addr_t* b)
{
  int i;
  struct ipv6_addr *at = (ipv6_addr *)a;
  struct ipv6_addr *bt = (ipv6_addr *)b;

  for(i=0; i<4; ++i) {
    if(ntohl(at->ip[i]) != bt->ip[i]) {
      return 0;
    }
  }
  return 1;
}

void convert(struct in6_addr* a, uip_ip6addr_t* b)
{
  int i;
  struct ipv6_addr *at = (ipv6_addr *)a;
  struct ipv6_addr *bt = (ipv6_addr *)b;

  for(i=0; i<4; ++i) {
    at->ip[i] = htonl(bt->ip[i]);
  }
}

void convertback(struct in6_addr* a, uip_ip6addr_t* b)
{
  int i;
  struct ipv6_addr *at = (ipv6_addr *)a;
  struct ipv6_addr *bt = (ipv6_addr *)b;

  for(i=0; i<4; ++i) {
    bt->ip[i] = ntohl(at->ip[i]);
  }
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
