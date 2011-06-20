#ifndef __HOAG_ACTION_H__
#define __HOAG_ACTION_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include "uip6.h"
#include "mobility.h"

#define MAX_NIO 200
#define MAX_KNOWN_GATEWAY 5

#define IP6_LEN  128
#define MAX_DEVNAME_SIZE     10
#define MAX_DEVNAME_NUM_SIZE 3

typedef struct hoag_gw_list {
  uint8_t used;
  uint8_t devnum;
//  uint16_t sequence;
  struct sockaddr_in6 hoag_addr;
  socklen_t hoag_addr_len;
} hoag_gw_list;

typedef struct hoag_nio_list {
  uint8_t used;
//  int gw;
  uip_lladdr_t addr;
} hoag_nio_list;

void hoag_init(int p, uip_ipaddr_t *pre, char* devname);
void hoag_receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len);
void hoag_close_tunnels(void);

//temp
void hoag_new_gw(struct mob_new_lbr *target);


#endif /* __HOAG_ACTION_H__ */
