#include "tunnel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *tldev = NULL;
char *tdev = NULL;

int
tunnel_client_create(char *tuneldev, char *tundev, const char *ipaddr, struct sockaddr_in6 *addr, socklen_t addr_len)
{
  char cmd[1024];
  char ip[128];
  int ret;

  tldev = tuneldev;
  tdev = tundev;

  if(inet_ntop(AF_INET6,(void*)(&addr->sin6_addr),&(ip[0]),8*sizeof(struct in6_addr)) == NULL) {
    printf("Impossible to understand incomming IP\n");
    return 42;
  }

  sprintf(cmd,"ip -6 tunnel add %s mode ip6ip6 remote %s local %s", tuneldev, ip, ipaddr);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }

  sprintf(cmd,"ip link set dev %s up", tuneldev);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }

  sprintf(cmd,"echo 42 %s.%s >> /etc/iproute2/rt_tables", tuneldev, tundev);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }

  sprintf(cmd,"ip rule add iif %s table %s.%s", tundev, tuneldev, tundev);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }

  sprintf(cmd,"ip -6 route add default via %s table %s.%s", ip, tuneldev, tundev);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }
  return 0;
}

int
tunnel_server_create(char *tuneldevbase, uint8_t tunneldev_int, const char *ipaddr, struct sockaddr_in6 *addr)
{
  char cmd[1024];
  char ip[128];
  int ret;

  if(inet_ntop(AF_INET6,(void*)(&addr->sin6_addr),&(ip[0]),8*sizeof(struct in6_addr)) == NULL) {
    printf("Impossible to understand incomming IP\n");
    return 42;
  }

  sprintf(cmd,"ip -6 tunnel add %s%u mode ip6ip6 remote %s local %s", tuneldevbase, tunneldev_int, ip, ipaddr);
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

  sprintf(cmd,"ip -6 route add %s dev %s%u", ip, tuneldevbase, tunneldev_int);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }
  return 0;
}

void
clear_tunnel(void) {
  char cmd[1024];
  int ret;
  if(tldev != NULL) {
    sprintf(cmd,"ip -6 tunnel del %s", tldev);
    printf("SH : %s\n",cmd);
    ret = system(cmd);
    sprintf(cmd,"ip rule del iif %s table %s.%s", tdev, tldev, tdev);
    printf("SH : %s\n",cmd);
    ret = system(cmd);
    sprintf(cmd,"ip -6 route flush table %s.%s", tldev, tdev);
    printf("SH : %s\n",cmd);
    ret = system(cmd);
    sprintf(cmd,"sed -i -e /42\\ %s.%s/d /etc/iproute2/rt_tables", tldev, tdev);
    printf("SH : %s\n",cmd);
    ret = system(cmd);
  }
}

void
close_tunnel(char* dev, uint8_t i) {
  char cmd[1024];
  int ret;
  sprintf(cmd,"ip -6 tunnel del %s%u", dev, i);
  printf("SH : %s\n",cmd);
  ret = system(cmd);
}
