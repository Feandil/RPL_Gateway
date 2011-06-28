#ifndef __TUNNEL_H__
#define __TUNNEL_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void tunnel_init(const char *ip, char* tundev, char* name, int num);
int tunnel_create(char *tuneldevbase, uint8_t tunneldev_int, struct sockaddr_in6 *addr);
void close_tunnel(char* dev, uint8_t i);

void create_reroute(char *tuneldevbase, uint8_t tunneldev_int);
void clear_reroute(void);

#endif /* __TUN_H__ */
