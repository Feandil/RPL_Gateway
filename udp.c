#include "udp.h"

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sys/perms.h"
#include "sys/event.h"
#include "mobility.h"
#include "tunnel.h"

#define DEBUG 1
#include "uip-debug.h"

int tunnel_created;

struct udp_io_t *udp_io;

void
udp_readable_cb (struct ev_loop *loop, struct ev_io *w, int revents)
{
  udp_io_t *udp_io;

  if (revents & EV_ERROR) {
    ev_io_stop (loop, w);
    printf("UDP error");
    return;
  }

  udp_io = (udp_io_t *)w;
  if(revents & EV_READ) {

    udp_io->read = recvfrom(w->fd, &udp_io->buffer, UDP_BUFF_SIZE, 0, (struct sockaddr *) &udp_io->addr, &udp_io->addr_len);

printf("read :");
PRINT6ADDR((uip_ipaddr_t*)&udp_io->addr.sin6_addr);
printf("port : %u\n", ntohs(udp_io->addr.sin6_port));

    if (udp_io->read > 1) {
      if (udp_io->read > 1280) {
        printf("UDP : Too large paquet\n");
      } else{
        receive_udp(&(udp_io->buffer[0]), udp_io->read, &udp_io->addr, udp_io->addr_len);
      }
    }

  }
  udp_io->read=0;
}

int
udp_init(int port)
{
  if(!perm_root_check()) {
    printf("Please run this function as root\n");
    return -1;
  }

  if ((udp_io=(udp_io_t *)malloc(sizeof(struct udp_io_t)))==NULL) {
    printf("malloc error");
    return -1;
  }
  memset(udp_io,0,sizeof(struct udp_io_t));

  udp_io->fd = socket(PF_INET6, SOCK_DGRAM, 0);

  if( udp_io->fd < 0 ) {
    printf("Unable to open ipv6 socket");
    return -1;
  }

  udp_io->addr.sin6_port = htons(port);
  udp_io->addr.sin6_family = AF_INET6;

  if (bind (udp_io->fd, (struct sockaddr *) &udp_io->addr, sizeof (udp_io->addr)) < 0) {
    printf("Unable to bind");
    return -1;
  }

  udp_io->addr.sin6_port=0;
  tunnel_created = 0;

  ev_io_init (&udp_io->udp_ev, udp_readable_cb, udp_io->fd, EV_READ);
  ev_io_start(event_loop,&udp_io->udp_ev);
  return 0;
}

void
udp_close(void)
{
  if(udp_io != NULL) {
    shutdown(udp_io->fd,2);
  }
}

void
udp_output(uint8_t *ptr, int size, struct sockaddr_in6 *addr)
{
  int res;

  printf("%u,%u,%u,%u", addr->sin6_family, ntohs(addr->sin6_port), addr->sin6_flowinfo, addr->sin6_scope_id);

printf("OUT1 :");
PRINT6ADDR((uip_ipaddr_t*)&addr->sin6_addr);
printf("port : %u\n", ntohs(udp_io->addr.sin6_port));
  res=sendto(udp_io->fd,ptr,size,0,(struct sockaddr *)addr, sizeof(struct sockaddr_in6));
  printf("udpout %i: %i\n",size,res);
}

struct sockaddr_in6*
udp_output_d(uint8_t *ptr, int size, uip_ipaddr_t *ipaddr, int port)
{
  int res;
  struct sockaddr_in6 *addr;
  if((addr = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6))) == NULL) {
    printf("MALLOC ERROR");
    return NULL;
  }

  addr->sin6_family = AF_INET6;

  addr->sin6_port = htons(port);
  addr->sin6_flowinfo = 0;
  memcpy(&addr->sin6_addr, ipaddr, sizeof(struct in6_addr));
  addr->sin6_scope_id = 0;

printf("OUT2 :");
PRINT6ADDR((uip_ipaddr_t*)&addr->sin6_addr);
printf("port : %u\n", ntohs(udp_io->addr.sin6_port));
  printf("%u,%u,%u,%u", addr->sin6_family, ntohs(addr->sin6_port), addr->sin6_flowinfo, addr->sin6_scope_id);
  res=sendto(udp_io->fd,ptr,size,0,(struct sockaddr *)addr, sizeof(struct sockaddr_in6));
  printf("udpout %i: %i\n",size,res);
  return addr;
}
