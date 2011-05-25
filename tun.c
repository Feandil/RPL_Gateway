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

int tun_fd;

int
tun_alloc(char *dev)
{
  struct ifreq ifr;
  int err;

  tun_fd = open("/dev/net/tun", O_RDWR);

  if( tun_fd < 0 ) {
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

  err = ioctl(tun_fd, TUNSETIFF, (void *) &ifr);
  if( err < 0 ) {
    close(tun_fd);
    return err;
  }
  strcpy(dev, ifr.ifr_name);
  return 0;
}

int
ifconf(char *tundev, const char *ipaddr)
{
  char cmd[128];
  int ret;

  sprintf(cmd,"ifconfig %s inet `hostname` up", tundev);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }

  sprintf(cmd,"ifconfig %s add %s", tundev, ipaddr);
  ret = system(cmd);
  if( ret < 0 ) {
    return ret;
  }
  sprintf(cmd,"ip -6 route add %s/64 dev %s", ipaddr, tundev);
  ret = system(cmd);
  return ret;
}

int
tun_create(char *tundev, const char *ipaddr)
{
  int ret;
  if(!perm_root_check()) {
    printf("Please run this function as root\n");
    return -1;
  }

  ret=tun_alloc(tundev);
  if( ret < 0 ) {
    return ret;
  }

  if(ifconf(tundev, ipaddr) < 0 ) {
    return -1;
  }
  return ret;
}

void
tun_output(uint8_t *ptr, int size) {
  int res;
  res=write(tun_fd,ptr,size);
  printf("tunout %i: %i",size,res);
}

