#ifndef __UDP_H__
#define __UDP_H__

#include <stdint.h>

#include "sys/event.h"
#include <sys/socket.h>
#include <netinet/in.h>
//#include <linux/in6.h>

#define UDP_BUFF_SIZE 1500

typedef struct udp_io_t {
  struct ev_io udp_ev;
  int fd;
  uint8_t buffer[UDP_BUFF_SIZE];
  struct sockaddr_in6 addr;
  socklen_t addr_len;
  int read;
} udp_io_t;

int udp_init(int port, char *tuneldev, char *tundev, char *ipaddr);
void udp_output(uint8_t *ptr, int size, struct sockaddr_in6 *addr);
void udp_close(void);

#endif /* __UDP_H__ */
