#ifndef __HOAG_ACTION_H__
#define __HOAG_ACTION_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include "uip6.h"

#define MAX_NIO 200
#define MAX_KNOWN_GATEWAY 5

#define IP6_LEN  128

typedef struct hoag_gw_list {
  uint8_t used;
  uint16_t sequence;
  struct sockaddr_in6 hoag_addr;
  socklen_t hoag_addr_len;
} hoag_gw_list;

typedef struct hoag_nio_list {
  uint8_t used;
  int gw;
  uip_lladdr_t addr;
} hoag_nio_list;

void hoag_init(int p, uip_ipaddr_t pre);
void hoag_receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len);

#endif /* __HOAG_ACTION_H__ */
