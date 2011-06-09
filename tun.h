#ifndef __TUN_H__
#define __TUN_H__

#include <stdint.h>

#include "sys/event.h"

#define TUN_BUFF_SIZE 1600

typedef struct tun_io_t {
  struct ev_io tun_ev;
  int fd;  /* FIle Descriptor */
  uint8_t buffer[TUN_BUFF_SIZE];
  int read;
} tun_io_t;

int tun_create(char *tundev, const char *ipaddr);
void tun_output(uint8_t *ptr, int size);

#endif /* __TUN_H__ */
