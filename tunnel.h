#ifndef __TUNNEL_H__
#define __TUNNEL_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int tunnel_client_create(char *tuneldev, char *tundev, const char *ipaddr, struct sockaddr_in6 *addr, socklen_t addr_len);
int tunnel_server_create(char *tuneldevbase, uint8_t tunneldev_int, const char *ipaddr, struct sockaddr_in6 *addr);
void clear_tunnel(void);
void close_tunnel(char* dev, uint8_t i);

#endif /* __TUN_H__ */
