#include "mob-action.h"

#include <stdio.h>
#include <string.h>
#include "mobility.h"
#include "sys/event.h"
#include "uip-ds6.h"
#include "udp.h"
#include "tunnel.h"

#define MOB_BUFF_OPT                    ((mob_opt *)(buff + temp_len))
#define MOB_BUFF_PREFIX          ((mob_opt_prefix *)(buff + temp_len))
#define MOB_BUFF_STATUS          ((mob_opt_status *)(buff + temp_len))
#define MOB_BUFF_PREF              ((mob_opt_pref *)(buff + temp_len))
#define MOB_BUFF_NIO                ((mob_opt_nio *)(buff + temp_len))
#define MOB_BUFF_TIMESTAMP    ((mob_opt_timestamp *)(buff + temp_len))

#define LLADDR_FROM_IPADDR(addr)    ((uip_lladdr_t *)(((uint8_t*)addr) + 8 ))

#define DEBUG 1
#include "uip-debug.h"

struct ev_timer* next_send;
struct mob_ll_list deleted[MAX_DELETE_NIO];
struct mob_ip_list gateways[MAX_KNOWN_GATEWAY];
uint8_t output_buffer[1280];

uint16_t sequence;

struct sockaddr_in6 hoag_addr;
socklen_t hoag_addr_len;


extern uip_ds6_route_t uip_ds6_routing_table[];

void
mob_send_message(struct ev_loop *loop, struct ev_timer *w, int revents)
{
  int temp_len, i;
  struct ext_hdr* hdr;
  struct mob_bind_up* data;
  uint8_t *buff;
  uint8_t handoff_status;

  if(hoag_addr_len == 0) {
    w->repeat = MOB_SEND_DELAY;
    printf("No Hoag, reporting send\n");
    ev_timer_again(loop,w);
    return;
  } else {
    w->repeat = 0;
  }

  hdr = (ext_hdr*) output_buffer;
  hdr->type = MOB_TYPE;
  hdr->mess = MOB_HR_UPDATE;
  data = (mob_bind_up*) &hdr->next;

  data->sequence = (++sequence);
  data->flag = 0;
  data->flag |= MOB_FLAG_UP_A;
  data->flag |= MOB_FLAG_UP_H;
  data->flag |= MOB_FLAG_UP_L;
  data->flag |= MOB_FLAG_UP_P;
  data->flag |= MOB_FLAG_UP_O;

  data->reserved = 0;

  data->lifetime = 0; /* Not implemented */

  buff = &(data->options);
  temp_len = 0;
  handoff_status = 0;

printf("Mob_up construction : ");

  for(i=0;i<UIP_DS6_ROUTE_NB;++i) {
    if(uip_ds6_routing_table[i].isused &&
        uip_ds6_routing_table[i].length == IP6_LEN &&
        uip_ds6_routing_table[i].state.pushed == 0) {
      if (handoff_status == 0) {
printf("Add Handoff (new)\n");
        MOB_BUFF_PREF->type = MOB_OPT_PREF;
        MOB_BUFF_PREF->len = MOB_LEN_HANDOFF - 2;
        MOB_BUFF_PREF->pref = 0;
        MOB_BUFF_PREF->hi = MOB_HANDOFF_NEW_BINDING;
        temp_len += MOB_LEN_HANDOFF;
        handoff_status = MOB_HANDOFF_NEW_BINDING;
      }
printf("Add NIO : ");
PRINT6ADDR(&uip_ds6_routing_table[i].ipaddr);
printf("\n");
      MOB_BUFF_NIO->type = MOB_OPT_NIO;
      MOB_BUFF_NIO->len = MOB_LEN_NIO - 2;
      MOB_BUFF_NIO->reserved = 0;
      memcpy(&MOB_BUFF_NIO->addr, LLADDR_FROM_IPADDR(&uip_ds6_routing_table[i].ipaddr), sizeof(uip_lladdr_t));
      temp_len += MOB_OPT_NIO;
    }
  }
  if (data->flag && MOB_FLAG_UP_K) printf("ERGGGGG 44");

  for(i=0;i<MAX_DELETE_NIO;++i) {
    if(deleted[i].used) {
      if (handoff_status != MOB_HANDOFF_UNKNOWN) {
printf("Add Handoff (unknown)\n");
        MOB_BUFF_PREF->type = MOB_OPT_PREF;
        MOB_BUFF_PREF->len = MOB_LEN_HANDOFF - 2;
        MOB_BUFF_PREF->pref = 0;
        MOB_BUFF_PREF->hi = MOB_HANDOFF_UNKNOWN;
        temp_len += MOB_LEN_HANDOFF;
        handoff_status = MOB_HANDOFF_UNKNOWN;
      }
printf("Del NIO\n");
      MOB_BUFF_NIO->type = MOB_OPT_NIO;
      MOB_BUFF_NIO->len = MOB_LEN_NIO - 2;
      MOB_BUFF_NIO->reserved = 0;
      memcpy(&MOB_BUFF_NIO->addr, &deleted[i].addr, sizeof(uip_lladdr_t));
      temp_len += MOB_OPT_NIO;
    }
  }
  if (data->flag && MOB_FLAG_UP_K) printf("ERGGGGG 42");

  if(temp_len != 0) {
    hdr->len = temp_len + MOB_LEN_BIND;
  if (data->flag && MOB_FLAG_UP_K) printf("ERGGGGG 43");

    udp_output(&output_buffer[0],hdr->len + MOB_LEN_HDR, &hoag_addr);
  }
}

int
mob_init(void)
{
  if((next_send = (ev_timer*) malloc(sizeof(ev_timer))) == NULL) {
    return 2;
  }
  hoag_addr_len = 0;
  ev_init(next_send,mob_send_message);
  return 0;
}

void
mob_connect_hoag(struct sockaddr_in6 *addr,socklen_t addr_len)
{
  memcpy(&hoag_addr, addr, sizeof(struct sockaddr_in6));
  hoag_addr_len = addr_len;
}

inline void
mob_maj_entry(struct uip_lladdr_t *nio, uint8_t status, uint8_t handoff)
{
  int i;

  if (!(status && MOB_STATUS_ERR_FLAG)) {
    switch(handoff) {
      case MOB_HANDOFF_UNKNOWN:
        for(i=0;i<MAX_DELETE_NIO;++i) {
          if(deleted[i].used &&
              (memcmp(nio,&deleted[i].addr,sizeof(uip_lladdr_t)) == 0)) {
            deleted[i].used = 0;
            return;
          }
        }
        break;
      default:
        for(i=0;i<UIP_DS6_ROUTE_NB;++i) {
          if(uip_ds6_routing_table[i].isused &&
              uip_ds6_routing_table[i].length == IP6_LEN &&
              (memcmp(LLADDR_FROM_IPADDR(&uip_ds6_routing_table[i].ipaddr),nio,sizeof(uip_lladdr_t)) == 0)) {
            uip_ds6_routing_table[i].state.pushed = 1;
            return;
          }
        }
    }
    printf("Uncleaned Value\n");
  }
}

inline void
mob_incoming_ack(uint8_t *buffer, int len) {

  mob_bind_ack *buff;
  uint8_t status;
  uint8_t handoff;
  int temp_len;
  struct uip_lladdr_t *nio = NULL;

  buff = (mob_bind_ack *) buffer;

  if (buff & MOB_FLAG_ACK_K) {
    printf("UDP IN : Flag unimplemented (K)");
    return;
  }

  if (buff & MOB_FLAG_ACK_R) {
    printf("UDP IN : Flag unimplemented (R)");
    return;
  }

  if (!buff & MOB_FLAG_ACK_P) {
    printf("UDP IN : Flag not set (P) (unimplemented)");
    return;
  }

  if (!buff & MOB_FLAG_ACK_O) {
    printf("UDP IN : Flag not set (O) (unimplemented)");
    return;
  }

  temp_len = sizeof(mob_bind_ack);
  status = buff->status;
  handoff = MOB_HANDOFF_NO_CHANGE;

  while (temp_len<len) {
    switch(MOB_BUFF_OPT->type) {
      case MOB_OPT_PREFIX:
        if(nio != NULL) {
          mob_maj_entry(nio,status,handoff);
          nio = NULL;
        }
        handoff = MOB_HANDOFF_NO_CHANGE;
        printf("Not Implemented");
        break;
      case MOB_OPT_STATUS:
        if(nio != NULL) {
          mob_maj_entry(nio,status,handoff);
          nio = NULL;
        }
        status = MOB_BUFF_STATUS->status;
        handoff = MOB_HANDOFF_NO_CHANGE;
        break;
      case MOB_OPT_PREF:
        if(nio != NULL) {
          mob_maj_entry(nio,status,handoff);
          nio = NULL;
        }
        handoff = MOB_BUFF_PREF->hi;
        printf("Not Implemented");
        break;
      case MOB_OPT_NIO:
        if(nio != NULL) {
          mob_maj_entry(nio,status,handoff);
        }
        nio = &MOB_BUFF_NIO->addr;
        break;
      case MOB_OPT_TIMESTAMP:
        printf("Not Implemented");
        break;
      default:
        printf("Unknown stuff");
        break;
    }
    temp_len += MOB_BUFF_OPT->len+2;
  }

  if(nio != NULL) {
    mob_maj_entry(nio,status,handoff);
  }
}

void
mob_receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len)
{
  ext_hdr *buff;

  if(read < MOB_HDR_LEN) {
    printf("UDP IN : Packet too short : %u\n", read);
    return;
  }

  buff=((ext_hdr*)buffer);

  if(buff->type != MOB_TYPE) {
    printf("UDP IN : Not a Mobility Packet\n");
    return;
  }

  if(buff->len != read - MOB_HDR_LEN) {
    printf("UDP IN : Bad length (%u VS %u - %u)\n", buff->len, read, MOB_HDR_LEN);
    return;
  }

  /*   WE SHOULD DO SOME AUTHENTIFICATION HERE    */

  switch(buff->mess) {
    case MOB_HR_UPDATE:
      printf("UDP IN : Bad message (Binding Update)\n");
      break;
    case MOB_HR_ACK:
      mob_incoming_ack(&buff->next,buff->len);
      break;
    case MOB_HR_NEW_G:
      printf("UDP IN : Bad message (New Gateway)\n");
      break;
    default:
      printf("UDP IN : Bad message (Unknown : %u)\n",buff->mess);
      break;
  }
}

void
receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len) {
  mob_receive_udp(buffer, read, addr, addr_len);
}


void
mob_send_lbr(uip_ip6addr_t *lbr) {
  struct ext_hdr* hdr;
  struct mob_new_lbr* data;

  hdr = (ext_hdr*) &output_buffer[0];
  hdr->type = MOB_TYPE;
  hdr->mess = MOB_HR_NEW_G;
  hdr->len = sizeof(mob_new_lbr);
  hdr->reserved = 0;
  data = (mob_new_lbr*) &hdr->next;

  data->reserved = 0;
  memcpy(&data->addr,lbr,sizeof(uip_ip6addr_t));
  udp_output(&output_buffer[0],hdr->len + MOB_LEN_HDR, &hoag_addr);
}

void
mob_new_6lbr(uip_ip6addr_t *lbr)
{
  int i;
  for(i=0;i<MAX_KNOWN_GATEWAY;++i) {
    if(gateways[i].used &&
        uip_ipaddr_cmp(lbr, &gateways[i].addr)) {
      return;
    }
  }
  for(i=0;i<MAX_KNOWN_GATEWAY;++i) {
    if(!gateways[i].used) {
      gateways[i].used = 1;
      memcpy(&gateways[i].addr,lbr,sizeof(uip_ip6addr_t));
      mob_send_lbr(lbr);
      return;
    }
  }
  printf("MOB : BUFFER OVERFLOW");
}


void
mob_new_node(void)
{
  printf("NEW NODE\n");
  printf("NEW NODE\n");
  printf("NEW NODE\n");
  if(!ev_is_active(next_send)) {
  printf("next send activated : %u",MOB_SEND_DELAY);
    ev_timer_set(next_send, MOB_SEND_DELAY, 0);
    ev_timer_start(event_loop, next_send);
  }
}

void
mob_lost_node(uip_ip6addr_t *lbr)
{
  int i;
  for(i=0;i<MAX_DELETE_NIO;++i) {
    if(deleted[i].used &&
        (memcmp(LLADDR_FROM_IPADDR(lbr),&deleted[i].addr,sizeof(uip_lladdr_t)) == 0)) {
      return;
    }
  }
  for(i=0;i<MAX_DELETE_NIO;++i) {
    if(!deleted[i].used) {
      deleted[i].used = 1;
      memcpy(&deleted[i].addr,LLADDR_FROM_IPADDR(lbr),sizeof(uip_lladdr_t));
      return;
    }
  }
  printf("MOB : BUFFER OVERFLOW");
}

void
udp_connected(struct sockaddr_in6 *addr, socklen_t addr_len, char* tldev, char* tdev, char *iaddr)
{
  tunnel_client_create(tldev,tdev,iaddr,addr,addr_len);
  mob_connect_hoag(addr,addr_len);
}
