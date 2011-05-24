#ifndef __TUN_H__
#define __TUN_H__

#include <stdint.h>

int tun_create(char *tundev, const char *ipaddr);
void tun_output(uint8_t *ptr, int size);

#endif /* __TUN_H__ */
