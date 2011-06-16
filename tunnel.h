#ifndef __TUNNEL_H__
#define __TUNNEL_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int tunnel_create(char *tuneldev, char *tundev, const char *ipaddr, struct sockaddr_in6 *addr, socklen_t addr_len);
void clear_tunnel(void);

#endif /* __TUN_H__ */
