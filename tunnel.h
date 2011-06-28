#ifndef __TUNNEL_H__
#define __TUNNEL_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void tunnel_server_init(const char *ip);
int tunnel_server_create(char *tuneldevbase, uint8_t tunneldev_int, struct sockaddr_in6 *addr);
void close_tunnel(char* dev, uint8_t i);

#endif /* __TUN_H__ */
