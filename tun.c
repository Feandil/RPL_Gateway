#include "tun.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sys/perms.h"
#include "sys/event.h"
#include "uip6.h"
#include "tcpip.h"

struct tun_io_t *tun_io;

void
tun_readable_cb (struct ev_loop *loop, struct ev_io *w, int revents)
{
  tun_io_t *tun_io;

  if (revents & EV_ERROR) {
    ev_io_stop (loop, w);
    return;
  }

  tun_io = (tun_io_t *)w;
  if(revents & EV_READ) {
    tun_io->read=read(w->fd,&tun_io->buffer,TUN_BUFF_SIZE);
    if (tun_io->read > 0) {
      if (tun_io->read > 1280) {
        printf("TUN : Too large paquet\n");
      } else{
        memcpy(uip_buf,tun_io->buffer,tun_io->read);
        uip_len = tun_io->read;
        tcpip_input();
      }
    }
  }
  tun_io->read=0;
}

int
tun_alloc(char *dev)
{
  struct ifreq ifr;
  int err;

  if ((tun_io=(tun_io_t *)malloc(sizeof(struct tun_io_t)))==NULL)
    return -1;
  memset(tun_io,0,sizeof(struct tun_io_t));

  tun_io->fd = open("/dev/net/tun", O_RDWR);

  if( tun_io->fd < 0 ) {
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));

  /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
   *        IFF_TAP   - TAP device
   *
   *        IFF_NO_PI - Do not provide packet information
   */
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if(*dev != 0)
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

  err = ioctl(tun_io->fd, TUNSETIFF, (void *) &ifr);
  if( err < 0 ) {
    close(tun_io->fd);
    return err;
  }
  strcpy(dev, ifr.ifr_name);

  ev_io_init (&tun_io->tun_ev, tun_readable_cb, tun_io->fd, EV_READ);
  ev_io_start(event_loop,&tun_io->tun_ev);
  return 0;
}

int
ifconf(char *tundev, const char *ipaddr)
{
  char cmd[128];
  int ret;

  sprintf(cmd,"ifconfig %s inet `hostname` up", tundev);
  printf("%s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }

  sprintf(cmd,"ifconfig %s add %s", tundev, ipaddr);
  printf("%s\n",cmd);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }
  sprintf(cmd,"ip -6 route add %s/64 dev %s", ipaddr, tundev);
  printf("%s\n",cmd);
  ret = system(cmd);
  return ret;
}

int
tun_create(char *tundev, const char *ipaddr )
{
  int ret;
  if(!perm_root_check()) {
    printf("Please run this function as root\n");
    return -1;
  }

  ret=tun_alloc(tundev);
  if( ret < 0 ) {
    printf("Tun alloc echec\n");
    return ret;
  }

  if(ifconf(tundev, ipaddr) < 0 ) {
    printf("Tun ifconf echec\n");
    return -1;
  }
  return ret;
}

void
tun_output(uint8_t *ptr, int size) {
  int res;
  res=write(tun_io->fd,ptr,size);
  printf("tunout %i: %i\n",size,res);
}

