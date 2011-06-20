#ifndef __MOB_ACTION_H__
#define __MOB_ACTION_H__

#include <sys/socket.h>
//#include <linux/in6.h>
#include <netinet/in.h>

#include "uip6.h"

#define MOB_SEND_DELAY 2
#define MAX_DELETE_NIO 20
#define MAX_KNOWN_GATEWAY 5

#define IP6_LEN  128

typedef struct mob_ll_list {
  uint8_t used;
  uip_lladdr_t addr;
} mob_ll_list;

typedef struct mob_ip_list {
  uint8_t used;
  uip_ipaddr_t addr;
} mob_ip_list;


int mob_init(void);
void mob_receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len);
void mob_connect_hoag(struct sockaddr_in6 *addr,socklen_t addr_len);
void mob_new_6lbr(uip_ip6addr_t *lbr);
void mob_new_node(void);
void mob_lost_node(uip_ip6addr_t *lbr);

#endif /* __MOB_ACTION_H__ */
