#include "tunnel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *tuntty = NULL;
char *tablename = NULL;
int tablenum = 0;
const char *locipaddr;
char cmd[1024];

void
tunnel_init(const char *ip, char* tundev, char* name, int num)
{
  locipaddr = ip;
  tablename = name;
  tablenum = num;
  tuntty = tundev;
}

int
tunnel_create(char *tuneldevbase, uint8_t tunneldev_int, struct sockaddr_in6 *addr)
{
  char cmd[1024];
  char ip[128];
  int ret;

  if(inet_ntop(AF_INET6,(void*)(&addr->sin6_addr),ip,8*sizeof(struct in6_addr)) == NULL) {
    printf("Impossible to understand incomming IP\n");
    return 42;
  }

  sprintf(cmd,"ip -6 tunnel add %s%u mode ip6ip6 remote %s local %s", tuneldevbase, tunneldev_int, ip, locipaddr);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }

  sprintf(cmd,"ip link set dev %s%u up", tuneldevbase, tunneldev_int);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }
  return 0;
}

void
close_tunnel(char* devbase, uint8_t i) {
  char cmd[1024];
  sprintf(cmd,"ip -6 route flush dev %s%u", devbase, i);
  printf("SH : %s\n",cmd);
  system(cmd);
  sprintf(cmd,"ip -6 tunnel del %s%u", devbase, i);
  printf("SH : %s\n",cmd);
  system(cmd);
}


void
create_reroute(char *tuneldevbase, uint8_t tunneldev_int)
{
  sprintf(cmd,"echo %u %s >> /etc/iproute2/rt_tables", tablenum, tablename);
  printf("SH : %s\n",cmd);
  system(cmd);

  sprintf(cmd,"ip -6 rule add unicast iif %s table %s", tuntty, tablename);
  printf("SH : %s\n",cmd);
  system(cmd);

  sprintf(cmd,"ip -6 route add default dev %s.%u table %s", tuneldevbase, tunneldev_int, tablename);
  printf("SH : %s\n",cmd);
  system(cmd);
}

void
clear_reroute(void)
{
  sprintf(cmd,"ip -6 rule del unicast iif %s table %s", tuntty, tablename);
  printf("SH : %s\n",cmd);
  system(cmd);
  sprintf(cmd,"ip -6 route flush table %s",tablename);
  printf("SH : %s\n",cmd);
  system(cmd);
  sprintf(cmd,"sed -i -e /%u\\ %s/d /etc/iproute2/rt_tables", tablenum, tablename);
  printf("SH : %s\n",cmd);
  system(cmd);
}

