#include "tunnel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *tldev = NULL;
char *tdev = NULL;

const char *locipaddr;

void
tunnel_server_init(const char *ip)
{
  locipaddr=ip;
}

int
tunnel_server_create(char *tuneldevbase, uint8_t tunneldev_int, struct sockaddr_in6 *addr)
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

//  sprintf(cmd,"ip -6 route add %s dev %s%u", ip, tuneldevbase, tunneldev_int);
//  printf("SH : %s\n",cmd);
//  ret = system(cmd);
//  if( ret < 0 ) {
//    return ret;
//  }
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
