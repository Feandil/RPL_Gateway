#include "tun.h"

#include <stdio.h>
#include <string.h>

#include "sys/perms.h"
#include "ttyConnection.h"
#include "sys/event.h"
#include "uip6.h"
#include "rpl/rpl.h"

int
main (int argc, char *argv[]) {

  char id[] = "tun0";
  char ip[] = "fc00::1";
  uip_ipaddr_t ipaddr;
  uip_ip6addr_t prefix;
  rpl_dag_t *dag;

  prefix.u8[0]=0xfc;
  prefix.u8[1]=0x00;
  prefix.u8[2]=0x00;
  prefix.u8[3]=0x00;
  prefix.u8[4]=0x00;
  prefix.u8[5]=0x00;
  prefix.u8[6]=0x00;

  event_init();

  uip_ds6_init();
  rpl_init();

  memcpy(&ipaddr,&prefix,8);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  dag = rpl_set_root(0,&ipaddr);
  if(dag != NULL) {
    rpl_set_prefix(dag, &prefix, 64);
  }




  perm_print();
  tun_create(id,ip);
//  perm_drop();
  perm_print();

  init_ttyUSBX(0);

  event_launch();

  return 0;
}
