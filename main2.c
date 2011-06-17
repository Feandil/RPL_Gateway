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
  clear_tunnel();
}

int
main (int argc, char *argv[]) {

  int port = 23423;
  char id[] = "tun0";
  char tun[] = "tunlbr";
  char ip[] = "fc00::1";
  char publicip[] = "2001:660:5301:18:223:dfff:fe8d:4efc";
  uip_ipaddr_t ipaddr;
  uip_ip6addr_t prefix;
  rpl_dag_t *dag;
  uip_ds6_route_t *rep;

  prefix.u8[0]=0xfc;
  prefix.u8[1]=0x00;
  prefix.u8[2]=0x00;
  prefix.u8[3]=0x00;
  prefix.u8[4]=0x00;
  prefix.u8[5]=0x00;
  prefix.u8[6]=0x00;
  prefix.u8[7]=0x00;


  signal(SIGINT, down);

  event_init();

  hoag_init();
  udp_init(port,&(tun[0]),&(id[0]),&(publicip[0]));

  event_launch();

  return 0;
}
