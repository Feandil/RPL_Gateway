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
#include "mob-action.h"
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

  uip_ds6_init();
  rpl_init();
  mob_init();

  memset(&ipaddr,0,16);
  memcpy(&ipaddr,&prefix,8);
  ipaddr.u8[15]=0x01;

  rep = uip_ds6_route_add(&ipaddr, 128, &ipaddr, 0);
  if (rep == NULL) {
    printf("Boot error");
    return -1;
  }
  rep->state.dag = NULL;
  rep->state.learned_from = RPL_ROUTE_FROM_INTERNAL;
  rep->state.pushed = 1;


  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  dag = rpl_set_root(0,&ipaddr);
  if(dag != NULL) {
    rpl_set_prefix(dag, &prefix, 64);
  }

//  dag->preference=0xff;

  perm_print();
  tun_create(id,ip);
//  perm_drop();
  perm_print();

  init_ttyUSBX(0);

  udp_init(port,tun,id,publicip);


  event_launch();


  return 0;
}
