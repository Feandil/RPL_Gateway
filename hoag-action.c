#include "hoag-action.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "mobility.h"
#include "sys/event.h"
#include "lib/tool.h"
#include "udp.h"
#include "tunnel.h"

#define DEBUG 1
#include "uip-debug.h"

#define MOB_BUFF_OPT                    ((mob_opt *)(buff + temp_len))
#define MOB_BUFF_PREFIX          ((mob_opt_prefix *)(buff + temp_len))
#define MOB_BUFF_STATUS          ((mob_opt_status *)(buff + temp_len))
#define MOB_BUFF_PREF              ((mob_opt_pref *)(buff + temp_len))
#define MOB_BUFF_NIO                ((mob_opt_nio *)(buff + temp_len))
#define MOB_BUFF_TIMESTAMP    ((mob_opt_timestamp *)(buff + temp_len))

#define LLADDR_FROM_IPADDR(addr)    ((uip_lladdr_t *)(((uint8_t*)addr) + 8 ))

struct hoag_nio_list nios[MAX_NIO];
struct hoag_gw_list   gws[MAX_KNOWN_GATEWAY];
uint8_t output_buffer[1280];

int port;
uip_ipaddr_t prefix;

char tuneldev[MAX_DEVNAME_SIZE];
uint8_t tunnelnum;


char cmd[512];


void
hoag_init(int p, uip_ipaddr_t *pre, char* devname)
{
  port = p;
  memcpy(&prefix,pre,sizeof(uip_ipaddr_t));

  strncpy(tuneldev,devname,MAX_DEVNAME_SIZE-1);
  tunnelnum = 0;
}

inline int
change_route(char *dest, int gw, char *command)
{
  sprintf(cmd,"ip -6 route %s %s dev %s%u", command, dest , tuneldev, gw);
  printf("SH : %s\n",cmd);
  return system(cmd);
}

inline uint8_t
apply_change_nio(uip_lladdr_t *nio, uint8_t handoff, uint8_t gw)
{
  char dest[128];

PRINT6ADDR(&prefix);
printf("\n");

  memcpy(((uint8_t*)&prefix)+sizeof(uip_lladdr_t),nio,sizeof(uip_lladdr_t));
  toString(&prefix,dest);

PRINT6ADDR(&prefix);
printf("\n");

  switch(handoff) {
    case MOB_HANDOFF_UNKNOWN:
        change_route(dest, gw, "del");
        break;
    default:
        change_route(dest, gw, "del");
        if(change_route(dest, gw, "add") < 0) {
          return MOB_STATUS_ERR_FLAG;
        }
        break;
  }
  return 0;
}

void
hoag_add_nio(uip_lladdr_t *nio, uint8_t handoff, uint8_t *buff, int *t_len, uint8_t *out_status, uint8_t *out_handoff, uint8_t gw)
{
  uint8_t status;
  int temp_len = *t_len;

  status = apply_change_nio(nio, handoff, gw);

  if(status != *out_status) {
    MOB_BUFF_STATUS->type = MOB_OPT_STATUS;
    MOB_BUFF_STATUS->len = MOB_LEN_STATUS - 2;
    MOB_BUFF_STATUS->reserved = 0;
    MOB_BUFF_STATUS->status = status;
    *out_status = status;
    temp_len += MOB_LEN_STATUS;
  }

  MOB_BUFF_NIO->type = MOB_OPT_NIO;
  MOB_BUFF_NIO->len  = MOB_LEN_NIO - 2;
  MOB_BUFF_NIO->reserved = 0;
  memcpy(&MOB_BUFF_NIO->addr,nio,sizeof(uip_lladdr_t));
  temp_len += MOB_LEN_NIO;

  *t_len = temp_len;
}

inline void
hoag_proceed_up(mob_bind_up *buffer, int len, struct sockaddr_in6 *addr, socklen_t addr_len)
{
  uint8_t handoff, out_handoff, out_status, *buff;
  int temp_len,i;
  struct mob_bind_ack *outbuff;
  uip_lladdr_t *nio;
  uint8_t gw;

  outbuff = (mob_bind_ack *)&output_buffer[0];
  nio = NULL;

  for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
    if(gws[i].used) {
      if(memcmp(&addr->sin6_addr,&gws[i].hoag_addr.sin6_addr,sizeof(struct in6_addr)) &&
          addr->sin6_port == gws[i].hoag_addr.sin6_port) {
        nio = (uip_lladdr_t *)&gw;
        gw = i;
        break;
      }
    }
  }
  if(nio == NULL) {
    printf("6LBR not found\n");
    return;
  }
  nio = NULL;

  if (!(buffer->flag & MOB_FLAG_UP_A)) {
    printf("UDP IN : Flag unimplemented (!A)\n");
    return;
  }

  if (!(buffer->flag & MOB_FLAG_UP_H)) {
    printf("UDP IN : Flag unimplemented (!H)\n");
    return;
  }

  if (!(buffer->flag & MOB_FLAG_UP_L)) {
    printf("UDP IN : Flag unimplemented (!L)\n");
    return;
  }

  if (buffer->flag & MOB_FLAG_UP_K) {
    printf("UDP IN : Flag unimplemented (K)\n");
    return;
  }

  if (buffer->flag & MOB_FLAG_UP_M) {
    printf("UDP IN : Flag unimplemented (M)\n");
    return;
  }

  if (buffer->flag & MOB_FLAG_UP_R) {
    printf("UDP IN : Flag unimplemented (R)\n");
    return;
  }

  if (!(buffer->flag & MOB_FLAG_UP_P)) {
    printf("UDP IN : Flag unimplemented (!P)\n");
    return;
  }

  if (!(buffer->flag & MOB_FLAG_UP_O)) {
    printf("UDP IN : Flag unimplemented (!O)\n");
    return;
  }

  /* TODO : Do some auth */
  /* TODO : verify sequence */

  outbuff->status = 0;
  outbuff->flag = 0;
  outbuff->flag |= MOB_FLAG_ACK_P;
  outbuff->flag |= MOB_FLAG_ACK_O;
  outbuff->sequence = buffer->sequence;
  outbuff->lifetime = 0;

  buff = &(buffer->options);
  temp_len = 0;
  handoff = MOB_HANDOFF_NO_CHANGE;
  out_handoff = MOB_HANDOFF_NO_CHANGE;
  out_status = 0;
  len -= MOB_LEN_BIND;

  while (temp_len<len) {
    switch(MOB_BUFF_OPT->type) {
      case MOB_OPT_PREFIX:
        if(nio != NULL) {
          hoag_add_nio(nio,handoff,buff,&temp_len,&out_status,&out_handoff,gw);
          nio = NULL;
        }
        handoff = MOB_HANDOFF_NO_CHANGE;
        printf("Not Implemented : MOB_OPT_PREFIX : %u \n",temp_len);
        break;
      case MOB_OPT_PREF:
        if(nio != NULL) {
          hoag_add_nio(nio,handoff,buff,&temp_len,&out_status,&out_handoff,gw);
          nio = NULL;
        }
        handoff = MOB_BUFF_PREF->hi;
        printf("Not Implemented : MOB_OPT_PREF : %u \n",temp_len);
        break;
      case MOB_OPT_NIO:
        printf("Implemented : MOB_OPT_NIO : %u \n",temp_len);
        if(nio != NULL) {
          hoag_add_nio(nio,handoff,buff,&temp_len,&out_status,&out_handoff,gw);
        }
        nio = &MOB_BUFF_NIO->addr;
        break;
      case MOB_OPT_TIMESTAMP:
        printf("Not Implemented : MOB_OPT_TIMESTAMP : %u \n",temp_len);
        break;
      default:
        printf("Unknown stuff : %u at %u \n",(MOB_BUFF_OPT->type),temp_len);
        break;
    }
    temp_len += MOB_BUFF_OPT->len+2;
  }

  if(nio != NULL) {
    hoag_add_nio(nio,handoff,buff,&temp_len,&out_status,&out_handoff,gw);
  }

  if (temp_len != 0) {


  }
}

void
hoag_new_gw(mob_new_lbr *target)
{
  int i;
  hoag_gw_list *unused_elt = NULL;
  char gw[128];

  toString(&target->addr,gw);

  for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
    if(gws[i].used) {
      if(!equal(&gws[i].hoag_addr.sin6_addr,&target->addr) == 0 ) {
        return;
      }
    } else if(unused_elt == NULL) {
      unused_elt = &gws[i];
    }
  }

  if(unused_elt == NULL) {
    printf("NO PLACE FOR NEW 6LBR !!!\n");
    return;
  }

  unused_elt->used = 1;
  unused_elt->hoag_addr.sin6_family = AF_INET6;

  unused_elt->hoag_addr.sin6_port = htons(port);
  unused_elt->hoag_addr.sin6_flowinfo = 0;
  memcpy(&unused_elt->hoag_addr.sin6_addr, &target->addr, sizeof(struct sockaddr_in6));
  unused_elt->hoag_addr.sin6_scope_id = 0;

  unused_elt->hoag_addr_len = sizeof(struct sockaddr_in6);

  output_buffer[0]='\n';
  output_buffer[1]='\0';

  udp_output(output_buffer, 1, &unused_elt->hoag_addr);
  udp_output(output_buffer, 1, &unused_elt->hoag_addr);

  unused_elt->devnum = tunnelnum++;
  tunnel_server_create(tuneldev, unused_elt->devnum, &unused_elt->hoag_addr);
  convert(&unused_elt->hoag_addr.sin6_addr,&target->addr);
}

void
hoag_delete_gw(int gw)
{
  gws[gw].used = 0;

  close_tunnel(tuneldev,gws[gw].devnum);
}

void
hoag_receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len)
{
  ext_hdr *buff;

  if(read < MOB_HDR_LEN) {
    printf("UDP IN : Packet too short : %u", read);
    return;
  }

  buff=((ext_hdr*)buffer);

  if(buff->type != MOB_TYPE) {
    printf("UDP IN : Not a Mobility Packet");
    return;
  }

  if(buff->len != read - MOB_HDR_LEN) {
    printf("UDP IN : Bad length (%u VS %u - %u)", buff->len, read, MOB_HDR_LEN);
    return;
  }

  /*   WE SHOULD DO SOME AUTHENTIFICATION HERE    */

  switch(buff->mess) {
    case MOB_HR_UPDATE:
      hoag_proceed_up((mob_bind_up*)&(buff->next), buff->len, addr, addr_len);
      break;
    case MOB_HR_ACK:
      printf("UDP IN : Bad message (Ack)\n");
      break;
    case MOB_HR_NEW_G:
      hoag_new_gw((mob_new_lbr*)&(buff->next));
      break;
    default:
      printf("UDP IN : Bad message (Unknown : %u)",buff->mess);
      break;
  }
}

void
receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len) {
  hoag_receive_udp(buffer, read, addr, addr_len);
}

void
udp_connected(struct sockaddr_in6 *addr, socklen_t addr_len, char* tldev, char* tdev, char *iaddr)
{
}

void
mob_new_6lbr(uip_ip6addr_t *lbr)
{
}


void
mob_new_node(void)
{
}

void
hoag_close_tunnels(void)
{
  int i;

  for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
    if(gws[i].used) {
      hoag_delete_gw(i);
    }
  }
}
