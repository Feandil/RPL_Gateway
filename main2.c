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

#define DEBUG 1
#include "uip-debug.h"

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
//  char distantip[] = "2001:660:5301:18:223:dfff:fe8d:4efc";
  uip_ip6addr_t ipaddr;
  uip_ip6addr_t prefix,privpref;
  struct mob_new_lbr target;

  ipaddr.u8[0] = 0x20;
  ipaddr.u8[1] = 0x01;
  ipaddr.u8[2] = 0x06;
  ipaddr.u8[3] = 0x60;
  ipaddr.u8[4] = 0x53;
  ipaddr.u8[5] = 0x01;
  ipaddr.u8[6] = 0x00;
  ipaddr.u8[7] = 0x18;
  ipaddr.u8[8] = 0x02;
  ipaddr.u8[9] = 0x23;
  ipaddr.u8[10] = 0xdf;
  ipaddr.u8[11] = 0xff;
  ipaddr.u8[12] = 0xfe;
  ipaddr.u8[13] = 0x8d;
  ipaddr.u8[14] = 0x4e;
  ipaddr.u8[15] = 0xfc;

  memcpy(&target.addr, &ipaddr,sizeof(uip_ip6addr_t));

PRINT6ADDR(&target.addr);
printf("\n");

  memset(&prefix,0,sizeof(uip_ip6addr_t));
  memset(&privpref,0,sizeof(uip_ip6addr_t));

  prefix.u8[0]=0xfc;
  privpref.u8[0]=0xfc;
  privpref.u8[1]=0x01;

  signal(SIGINT, down);

printf("laucnh\n");
  event_init();

printf("laucnh\n");
  udp_init(port,NULL,NULL,publicip);
printf("laucnh\n");
  tunnel_server_init(publicip);
printf("laucnh\n");
  hoag_init(distantport,(uip_ipaddr_t *)&prefix,tun);
printf("laucnh\n");
  hoag_new_gw(&target);

  event_launch();

  return 0;
}
