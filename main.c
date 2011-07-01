#include <netinet/in.h>

#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "sys/perms.h"
#include "ttyConnection.h"
#include "sys/event.h"
#include "uip6.h"
#include "uiplib.h"
#include "rpl/rpl.h"
#include "mobility.h"
#include "udp.h"
#include "tunnel.h"
#include "tun.h"

void
print_help()
{
  printf("Usage : ");
  printf("prog -p port -l lipv6 -t tundev -u tundev -P prefix -Q privprefix -i ip6 -y ttydev [-abr] [-c distant_ip6 -T tablename -R tablenum]\n");
  printf("\t-h : Print this message\n");
  printf("\t-p port : Specify the port used for the routing protocol\n");
  printf("\t-l lipv6 : Specify the local ipv6 address\n");
  printf("\t-t tundev : Specify the base name for tunnel devices (tundevX)\n");
  printf("\t-y ttydev : Specify the number of the ttyUSB that connect to the device (default : 0)\n");
  printf("\t-u tundev : Specify the name of the tun used for the output of the ttydevice\n");
  printf("\t-a : Use this node as the principal home agent\n");
  printf("\t-b : Use this node as the backup home agent\n");
  printf("\t-r : Use this node as a simple router \n");
  printf("\t-c distant_ip6 : connect to the specified ipv6\n");
  printf("\t-P prefix : the prefix used for the inside of the 6LowPAN (Must be an ip, considered as a /64)\n");
  printf("\t-Q privprefix : the private prefix used for the tun devices (Must be an ip, considered as a /64)\n");
  printf("\t-T tablename : name of iproute2 table used for outgoing traffic\n");
  printf("\t-R tablenum : int designating the iproute2 table used for outgoing traffic\n");
}

void
down(int sig)
{
  printf("\ndie\n");
  event_stop();
  udp_close();
  mob_close_tunnels();
  tun_close();
}

int
main (int argc, char *argv[])
{
  uint8_t state = 0;
  int port = 0,
      tty = 0,
      tablenum = 0;
  char *tunhoag = NULL,
       *tuntty = NULL,
       *localip = NULL,
       *publicip = NULL,
       *prefix = NULL,
       *privprefix = NULL,
       *distantip = NULL,
       *tablename = NULL;
  uip_ipaddr_t ipaddr,
               pref,
               privpref,
               distant,
               local;
  rpl_dag_t *dag;
  uip_ds6_route_t *rep;

  char opt_char=0;
  while ((opt_char = getopt(argc, argv, "hp:l:t:y:u:abrc:P:Q:i:T:R:")) != -1) {
    switch(opt_char) {
      case 'h':
        print_help();
        return(0);
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'l':
        publicip = optarg;
        break;
      case 't':
        tunhoag = optarg;
        break;
      case 'y':
        tty = atoi(optarg);
        break;
      case 'u':
        tuntty = optarg;
        break;
      case 'a':
        if(state == 0) {
          state |= MOB_TYPE_STORE;
          state |= MOB_TYPE_APPLY;
        } else {
          printf("a,b or r option are not supposed to be used at the same time");
          return -1;
        }
        break;
      case 'b':
        if(state == 0) {
          state |= MOB_TYPE_STORE;
          state |= MOB_TYPE_UPWARD;
        } else {
          printf("a,b or r option are not supposed to be used at the same time");
          return -1;
        }
        break;
      case 'r':
        if(state == 0) {
          state |= MOB_TYPE_UPWARD;
        } else {
          printf("a,b or r option are not supposed to be used at the same time");
          return -1;
        }
        break;
      case 'c':
        distantip = optarg;
        break;
      case 'P':
        prefix = optarg;
        break;
      case 'Q':
        privprefix = optarg;
        break;
      case 'i':
        localip = optarg;
        break;
      case 'R':
        tablenum = atoi(optarg);
        break;
      case 'T':
        tablename = optarg;
        break;
      default:
        print_help();
        return(0);
        break;
    }
  }

  if(port == 0) {
    printf("Error : port not set (use -p)\n");
    return(-1);
  }
  if(publicip == NULL || !uiplib_ipaddrconv(publicip,&ipaddr)) {
    printf("Error : local ipv6 address not set (use -l)\n");
    return(-1);
  }
  if(tunhoag == NULL) {
    printf("Error : base name for tunnel devices not set (use -t)\n");
    return(-1);
  }
  /* tty : no verification */
  if(tuntty == NULL) {
    printf("Error : name for local tun for tty device not net\n");
    return(-1);
  }
  if(state == 0) {
    printf("error : null state\n");
    return(-1);
  }
  if((distantip != NULL && !uiplib_ipaddrconv(distantip,&distant))) {
    printf("Error : The 6LBR address used for connecting with the lln isn't valid\n");
    return(-1);
  }
  if((prefix == NULL) || !uiplib_ipaddrconv(prefix,&pref)) {
    printf("Error : The prefix MUST be set\n");
    return(-1);
  }
  if((privprefix == NULL) || !uiplib_ipaddrconv(privprefix,&privpref)) {
    printf("Error : The private prefix MUST be set\n");
    return(-1);
  }
  if((localip == NULL) || !uiplib_ipaddrconv(localip,&local)) {
    printf("Error : The ipv6 of the virtual node be set\n");
    return(-1);
  }
  if(memcmp(&local,&privpref,sizeof(uip_lladdr_t))) {
    printf("The ipv6 of the virtual node is not defined inside the private prefix\n");
    return(-1);
  }
  if(state & MOB_TYPE_UPWARD && (tablename == NULL || tablenum == 0)) {
   printf("iproute2 table undefined");
  }

  signal(SIGINT, down);

  event_init();


/* Initialize internal node */
  uip_ds6_init();
  rpl_init();

  rep = uip_ds6_route_add(&local, 128, &local, 0);
  if (rep == NULL) {
    printf("Boot error");
    return -1;
  }
  rep->state.dag = NULL;
  rep->state.learned_from = RPL_ROUTE_FROM_INTERNAL;


  uip_ds6_set_addr_iid(&local, &uip_lladdr);
  dag = rpl_set_root(0,&ipaddr);
  if(dag != NULL) {
    rpl_set_prefix(dag, &pref, 64);
  }
//  dag->preference=0xff;

/* Initialize Mobility routing protocol */
  mob_init(state, port, &pref, &ipaddr, tunhoag, tuntty);

/* Initialize connection with the node */
  tuntty=tun_create(tuntty,localip);
  if(tuntty == NULL) {
    return -1;
  }
  init_ttyUSBX(tty);

/* Tunnels initialisation */
  tunnel_init(publicip,tuntty,tablename,tablenum);

/* initialize UDP connection for mobility routing protocol */
  udp_init(port);

/* If we need to connect to an another node, do it */
  if(distantip != NULL) {
    mob_init_lbr(&distant);
  }

/* Let's rock */
  event_launch();
  return 0;
}
