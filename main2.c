#include <netinet/in.h>

#include "main.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "sys/perms.h"
#include "ttyConnection.h"
#include "sys/event.h"
#include "uip6.h"
#include "rpl/rpl.h"
#include "hoag-action.h"
#include "udp.h"
#include "tunnel.h"
#include "tun.h"

void
down(int sig) {
  printf("\ndie\n");
  event_stop();
  udp_close();
  hoag_close_tunnels();
}

int
main (int argc, char *argv[]) {

  int port = 23423;
  int distantport = 23423;
  char tun[] = "tunlbr";
  char publicip[] = "2001:660:5301:18:4a5b:39ff:fe19:3bd5";
  char distantip[] = "2001:660:5301:18:223:dfff:fe8d:4efc";
  uip_ipaddr_t ipaddr;
  uip_ip6addr_t prefix;

  memset(&prefix,0,sizeof(uip_ip6addr_t));

  prefix.u8[0]=0xfc;

  signal(SIGINT, down);

  event_init();

  tunnel_server_init(publicip);
  hoag_init(distantport,(uip_ipaddr_t *)&prefix,&tun);
  udp_init(port,NULL,NULL,publicip);

  event_launch();

  return 0;
}
