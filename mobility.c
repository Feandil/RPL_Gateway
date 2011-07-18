#include "mobility.h"
#include "mobility-priv.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sys/event.h"
#include "lib/tool.h"
#include "udp.h"
#include "tunnel.h"

#define DEBUG 1
#include "uip-debug.h"

#define HARDDEBUG 0

#define MOB_BUFF_OPT                    ((mob_opt *)(((uint8_t*)buff) + temp_len))
#define MOB_BUFF_PREFIX          ((mob_opt_prefix *)(((uint8_t*)buff) + temp_len))
#define MOB_BUFF_STATUS          ((mob_opt_status *)(((uint8_t*)buff) + temp_len))
#define MOB_BUFF_PREF              ((mob_opt_pref *)(((uint8_t*)buff) + temp_len))
#define MOB_BUFF_NIO                ((mob_opt_nio *)(((uint8_t*)buff) + temp_len))
#define MOB_BUFF_TIMESTAMP    ((mob_opt_timestamp *)(((uint8_t*)buff) + temp_len))

#define LLADDR_FROM_IPADDR(addr)    ((uip_lladdr_t*)(((uint8_t*)addr) + sizeof(uip_lladdr_t)))
#define UINT_LESS_THAN(a,b)           (((uint16_t)(b-a))<(UINT16_MAX/2))

char cmd[512];
char tuneldev[MAX_DEVNAME_SIZE];
char ttydev[MAX_DEVNAME_SIZE];
uint8_t mob_type;
uint8_t tunnelnum;
uint8_t output_buffer[1280];
uint16_t min_out_seq;
uint16_t min_ack_sequence;
uint16_t max_out_seq;
uint16_t next_out_sequence;

uint16_t min_in_seq;
uint16_t max_in_seq;

int port;
uip_ipaddr_t prefix, myip;
struct uip_lladdr_list deleted[MAX_DELETE_NIO];
struct gw_list gws[MAX_KNOWN_GATEWAY];

struct ev_timer *next_send;
struct ev_timer *next_cleaner;
struct gw_list *hoag_pri, *hoag_bk;

extern uip_ds6_route_t uip_ds6_routing_table[];

int
change_route(uint8_t gw, char *command)
{
  if(mob_type & MOB_TYPE_APPLY) {
    char dest[128];
    toString(&prefix,dest);
    sprintf(cmd,"ip -6 route %s %s dev %s%u", command, dest , tuneldev, gws[gw].devnum);
    printf("SH : %s\n",cmd);
    return system(cmd);
  } else {
    return 0;
  }
}

int
change_local_route(char *command)
{
  char dest[128];
  toString(&prefix,dest);
  sprintf(cmd,"ip -6 route %s %s dev %s", command, dest , ttydev);
  printf("SH : %s\n",cmd);
  return system(cmd);
}

void
mob_add_nio(uip_lladdr_t *nio, uint8_t handoff, uint8_t *buff, int *t_len, uint8_t *out_status, uint8_t *out_handoff, uint8_t gw, uint64_t stamp)
{
  uint8_t status = 0;
  int temp_len = *t_len;
  uip_ds6_route_t *locroute;

  memcpy(((uint8_t*)&prefix)+sizeof(uip_lladdr_t),nio,sizeof(uip_lladdr_t));

printf("ADD NIO : ");
          PRINT6ADDR(&prefix);
          printf("\n");

  for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
      locroute++) {
    if(locroute->isused) {
      /* Do not change local routes */
      if(uip_ipaddr_cmp(&prefix,&locroute->ipaddr)) {
        if(((stamp != 0)
            && ((locroute->state.lifetime.start == 0)
                || (stamp > (uint64_t) locroute->state.lifetime.start)))
              || (stamp == 0 && locroute->state.lifetime.start == 0)) {
          printf("Received distant Node newer than local node (");
          PRINT6ADDR(&prefix);
          printf("):\n");
          printf("                      Local stimestamp : %lu\n", locroute->state.lifetime.start);
          printf("                      Remote (%u) stimestamp : %"PRIu64"\n", gw, stamp);
          printf("-> DELETE PREVIOUS ENTRY\n");
          break;
        } else {
          printf("Received distant Node older than local node (");
          PRINT6ADDR(&prefix);
          printf("):\n");
          printf("                      Local stimestamp : %lu\n", locroute->state.lifetime.start);
          printf("                      Remote (%u) stimestamp : %"PRIu64"\n", gw, stamp);
          printf("IGNORED\n");
          return;
        }
      }
    }
  }


  printf("Add distant Node : \n");
  printf("                   Address : ");
  PRINT6ADDR(&prefix);
  printf("\n");
  printf("                   Gateway : ");
  PRINT6ADDR((uip_ipaddr_t*)&gws[gw].addr.sin6_addr);
  printf(" (%u)\n",gw);
  printf("                   Stimestamp : %"PRIu64"\n", stamp);

  switch(handoff) {
    case MOB_HANDOFF_UNKNOWN:
    {
      if((mob_type & MOB_TYPE_APPLY)
          && (locroute != uip_ds6_routing_table + UIP_DS6_ROUTE_NB)) {
        if(locroute->state.gw != gw) {
          printf("ERROR : NIO was deleted by the wrong 6LBR\n");
        } else {
          change_route(gw, "del");
          uip_ds6_route_rm(locroute);
        }
      }
      break;
    }
    default:
    {
      if(locroute != uip_ds6_routing_table + UIP_DS6_ROUTE_NB) {
        if(locroute->state.learned_from == RPL_ROUTE_FROM_6LBR) {
          if(locroute->state.gw == gw) {
            break;
          }
          change_route(locroute->state.gw, "del");
        } else {
          mob_lost_node(locroute);
          change_route(gw, "add");
          locroute->state.gw = gw;
          locroute->state.lifetime.start = stamp;
        }
      } else {
        if(change_route(gw, "add") < 0) {
//          status = MOB_STATUS_ERR_FLAG;
          printf("AN ERROR OCCURED\n");
        }
        locroute = uip_ds6_route_add(&prefix, IP6_LEN, &prefix, 0); /* TODO : change this 0 to pref */
        if(locroute != NULL) {
          locroute->state.learned_from = RPL_ROUTE_FROM_6LBR;
          locroute->state.gw = gw;
          locroute->state.lifetime.start = stamp;
        } else {
          change_route(gw, "del");
//          status = MOB_STATUS_ERR_FLAG;
          printf("AN ERROR OCCURED\n ");
        }
      }
      break;
    }
  }

  if(status != *out_status) {
    MOB_BUFF_STATUS->type = MOB_OPT_STATUS;
    MOB_BUFF_STATUS->len  = MOB_LEN_STATUS - 2;
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

void
mob_proceed_up(mob_bind_up *buffer, int len, struct sockaddr_in6 *addr, socklen_t addr_len)
{
  uint8_t handoff, out_handoff, out_status, *buff;
  int temp_len,out_len,i;
  struct ext_hdr *hdr;
  struct mob_bind_ack *outbuff;
  uip_lladdr_t *nio;
  uint8_t gw;

  nio = NULL;

  for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
    if(gws[i].used == MOB_GW_KNOWN) {
      if(memcmp(&addr->sin6_addr,&gws[i].addr.sin6_addr,sizeof(struct in6_addr)) == 0) {
        gw = i;
        nio = (uip_lladdr_t *)&gw;
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

  hdr = (ext_hdr*) output_buffer;
  outbuff = (mob_bind_ack *) &hdr->next;
  buff = &(buffer->options);

  /* TODO : verify sequence */

  temp_len = 0;
  handoff = MOB_HANDOFF_NO_CHANGE;
  out_len = 0;
  out_handoff = MOB_HANDOFF_NO_CHANGE;
  out_status = 0;
  len -= MOB_LEN_BIND;

  printf("Receive udapte for seq %u from 6lbr %u\n", buffer->sequence, gw);

  while (temp_len<len) {
   printf("Pos : %u/%u\n", temp_len,len);
    switch(MOB_BUFF_OPT->type) {
      case MOB_OPT_PADN:
        break;
      case MOB_OPT_PREFIX:
        if(nio != NULL) {
          mob_add_nio(nio,handoff,&(outbuff->options),&out_len,&out_status,&out_handoff,gw,0);
          nio = NULL;
        }
        handoff = MOB_HANDOFF_NO_CHANGE;
        printf("Not Implemented : MOB_OPT_PREFIX : %u \n",temp_len);
        break;
      case MOB_OPT_PREF:
        if(nio != NULL) {
          mob_add_nio(nio,handoff,&(outbuff->options),&out_len,&out_status,&out_handoff,gw,0);
          nio = NULL;
        }
        handoff = MOB_BUFF_PREF->hi;
        printf("Not Implemented : MOB_OPT_PREF : %u \n",temp_len);
        break;
      case MOB_OPT_NIO:
        printf("Implemented : MOB_OPT_NIO : %u, value : ",temp_len);
        PRINTLLADDR(&MOB_BUFF_NIO->addr);
        printf("\n");
        if(nio != NULL) {
          mob_add_nio(nio,handoff,&(outbuff->options),&out_len,&out_status,&out_handoff,gw,0);
        }
        nio = &MOB_BUFF_NIO->addr;
        break;
      case MOB_OPT_TIMESTAMP:
        printf("Implemented : MOB_OPT_TIMESTAMP : %u, value : %"PRIu64"\n",temp_len,MOB_BUFF_TIMESTAMP->timestamp);
        if(nio != NULL) {
          mob_add_nio(nio,handoff,&(outbuff->options),&out_len,&out_status,&out_handoff,gw,MOB_BUFF_TIMESTAMP->timestamp);
          nio = NULL;
        } else {
          printf("ERROR : MOB_OPT_TIMESTAMP without a correct MOB_OPT_NIO : %u \n",temp_len);
        }
        break;
      default:
        printf("Unknown stuff : %u at %u \n",(MOB_BUFF_OPT->type),temp_len);
        break;
    }
    temp_len += MOB_BUFF_OPT->len+2;
  }

  if(nio != NULL) {
    mob_add_nio(nio,handoff,&(outbuff->options),&out_len,&out_status,&out_handoff,gw,0);
  }

  hdr->type = MOB_TYPE;
  hdr->mess = MOB_HR_ACK;
  hdr->len = out_len + MOB_LEN_ACK;
  outbuff->status = 0;
  outbuff->flag = 0;
  outbuff->flag |= MOB_FLAG_ACK_P;
  outbuff->flag |= MOB_FLAG_ACK_O;
  outbuff->sequence = buffer->sequence;
  outbuff->lifetime = 0;

  printf("send ack for seq %u from 6lbr %u\n", outbuff->sequence, gw);
  udp_output(output_buffer,hdr->len + MOB_LEN_HDR, addr);
}

void
mob_send_message(struct ev_loop *loop, struct ev_timer *w, int revents)
{
  int temp_len, i, j;
  struct ext_hdr* hdr;
  struct mob_bind_up* data;
  uint8_t *buff;
  uint8_t handoff_status;
  uint16_t pos;
  uint8_t sent;
#if HARDDEBUG
uip_ds6_route_t *locroute;
#endif /* HARDDEBUG*/

  if(gws[0].used != MOB_GW_KNOWN) {
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

  sent = 0;

#if HARDDEBUG
printf("Current min/max : %u/%u",min_out_seq,max_out_seq);
   for(locroute = uip_ds6_routing_table;
          locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
          locroute++) {
        printf("route %u : ",(uint16_t)(locroute - uip_ds6_routing_table));
        if(locroute->isused
            && locroute->state.learned_from == RPL_ROUTE_FROM_UNICAST_DAO) {
          printf("used : seq = %u, addr = ",locroute->state.seq);
          PRINT6ADDR(&locroute->ipaddr);
          printf(" prev : %u, next : %u\n",locroute->state.prev_seq,locroute->state.next_seq);
        } else {
          printf("unused\n");
        }
      }
printf("Current min/max : %u/%u",min_out_seq,max_out_seq);
#endif /* HARDDEBUG*/

  mob_check_all_entry();

  for(i=0; i <= MAX_LBR_BACKUP; ++i) {
    if(gws[i].used == MOB_GW_KNOWN &&
       ((uint16_t)(next_out_sequence - gws[i].sequence_out) > 1)) {
      if(!(gws[i].non_ack < MAX_NON_ACK)) {
         mob_delete_gw(i);
         break;
      }
      if((uint16_t)(next_out_sequence - gws[i].sequence_out) > MOB_MAX_ENTRY_BY_UP + 1) {
        data->sequence = gws[i].sequence_out + MOB_MAX_ENTRY_BY_UP;
      } else {
        data->sequence = next_out_sequence - 1;
      }
      printf("Mob_up construction : ");

      pos = min_out_seq;
      while(pos != MOB_LIST_END
          && UINT_LESS_THAN(uip_ds6_routing_table[pos].state.seq, gws[i].sequence_out)) {
        pos = uip_ds6_routing_table[pos].state.next_seq;
      }

      printf("Starting pos : %u", pos);
      if(pos != MOB_LIST_END) {
        printf(" (ie seq %u)\n",uip_ds6_routing_table[pos].state.seq);
      } else {
        printf("\n");
      }
      printf("Theorical end : %u \n",data->sequence);

      while(pos != MOB_LIST_END
          && UINT_LESS_THAN(uip_ds6_routing_table[pos].state.seq, data->sequence + 1)) {
        if(handoff_status == 0) {
printf("Add Handoff (new) : %u \n",temp_len);
          MOB_BUFF_PREF->type = MOB_OPT_PREF;
          MOB_BUFF_PREF->len = MOB_LEN_HANDOFF - 2;
          MOB_BUFF_PREF->pref = 0;
          MOB_BUFF_PREF->hi = MOB_HANDOFF_NEW_BINDING;
          temp_len += MOB_LEN_HANDOFF;
          handoff_status = MOB_HANDOFF_NEW_BINDING;
        }
printf("Add NIO :  %u : ",temp_len);
PRINT6ADDR(&uip_ds6_routing_table[pos].ipaddr);
        MOB_BUFF_NIO->type = MOB_OPT_NIO;
        MOB_BUFF_NIO->len = MOB_LEN_NIO - 2;
        MOB_BUFF_NIO->reserved = 0;
        memcpy(&MOB_BUFF_NIO->addr, LLADDR_FROM_IPADDR(&uip_ds6_routing_table[pos].ipaddr), sizeof(uip_lladdr_t));
printf(" : ");
PRINTLLADDR(&MOB_BUFF_NIO->addr);
printf("\n");
        temp_len += MOB_LEN_NIO;

printf("Add PadN (2) :  %u \n", temp_len);
        MOB_BUFF_OPT->type = MOB_OPT_PADN;
        MOB_BUFF_OPT->len = 0;
        temp_len += 2;

printf("Add timestamp : %u\n", temp_len);
        MOB_BUFF_TIMESTAMP->type = MOB_OPT_TIMESTAMP;
        MOB_BUFF_TIMESTAMP->len = MOB_LEN_TIMESTAMP -2;
        MOB_BUFF_TIMESTAMP->timestamp = uip_ds6_routing_table[pos].state.lifetime.start;
        temp_len += MOB_LEN_TIMESTAMP;

        pos = uip_ds6_routing_table[pos].state.next_seq;
      }

      for(j=0;j<MAX_DELETE_NIO;++j) {
        if(deleted[j].used &&
            UINT_LESS_THAN((deleted[j].seq - data->sequence),MOB_MAX_ENTRY_BY_UP)) {
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
          memcpy(&MOB_BUFF_NIO->addr, &deleted[j].addr, sizeof(uip_lladdr_t));
          temp_len += MOB_LEN_NIO;
        }
      }

printf("End position : %u\n",temp_len);
      if(temp_len != 0) {
        hdr->len = temp_len + MOB_LEN_BIND;
        printf("Send Update for seq %u from 6lbr %u\n", data->sequence, i);
        udp_output(output_buffer,hdr->len + MOB_LEN_HDR, &gws[i].addr);
        ++gws[i].non_ack;
        sent = 1;
      }
    }
  }
  if(sent) {
    w->repeat = MOB_SEND_TIMEOUT;
    ev_timer_again(loop,w);
  }
}

inline void
mob_incoming_ack(mob_bind_ack *buffer, int len, struct sockaddr_in6 *addr)
{
  uint8_t gw;
  uint16_t last_ack;
  int i;
  struct uip_lladdr_t *nio;

/*
  uint8_t status, handoff, *buff;
  int temp_len;
*/


  if (buffer->flag & MOB_FLAG_ACK_K) {
    printf("UDP IN : Flag unimplemented (K)");
    return;
  }

  if (buffer->flag & MOB_FLAG_ACK_R) {
    printf("UDP IN : Flag unimplemented (R)");
    return;
  }

  if (!(buffer->flag & MOB_FLAG_ACK_P)) {
    printf("UDP IN : Flag not set (P) (unimplemented)");
    return;
  }

  if (!(buffer->flag & MOB_FLAG_ACK_O)) {
    printf("UDP IN : Flag not set (O) (unimplemented)");
    return;
  }

  nio = NULL;

  for(i = 0; i < MAX_LBR_BACKUP + 1; ++i) {
    if(gws[i].used == MOB_GW_KNOWN) {
      if(memcmp(&addr->sin6_addr,&gws[i].addr.sin6_addr,sizeof(struct in6_addr)) == 0) {
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

  if(min_ack_sequence == gws[gw].sequence_out) {
    last_ack = min_ack_sequence;
    gws[gw].sequence_out = buffer->sequence;
    min_ack_sequence = next_out_sequence;
    for(i = 0; i < MAX_LBR_BACKUP + 1; ++i) {
      if(gws[i].used == MOB_GW_KNOWN
          && UINT_LESS_THAN(gws[i].sequence_out, min_ack_sequence)) {
        min_ack_sequence = gws[i].sequence_out;
      }
    }
    for(i=0; i<MAX_DELETE_NIO; ++i) {
      if(deleted[i].used
          && UINT_LESS_THAN(last_ack,deleted[i].seq)
          && UINT_LESS_THAN(deleted[i].seq,min_ack_sequence)) {
        deleted[i].used = 0;
      }
    }
  } else {
    gws[gw].sequence_out = buffer->sequence;
  }

  gws[gw].sequence_out = buffer->sequence;
  gws[gw].non_ack = 0;
  printf("Ack for sequence %u received from 6LBR %u", gws[gw].sequence_out, gw);

/*
  status = ack_buff->status;
  handoff = MOB_HANDOFF_NO_CHANGE;
  temp_len = 0;
  buff = &(ack_buff->options);
  len -= MOB_LEN_ACK;

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
        printf("Unknown stuff : %u, pos : %u, len : %u",MOB_BUFF_OPT->type,temp_len, MOB_BUFF_OPT->len+2);
        break;
    }
    temp_len += MOB_BUFF_OPT->len+2;
  }

  if(nio != NULL) {
    mob_maj_entry(nio,status,handoff);
  }
  */
}


void
mob_send_lbr(uip_ip6addr_t *lbr, uint8_t flag)
{
  int i;
  struct ext_hdr* hdr;
  struct mob_new_lbr* data;

  hdr = (ext_hdr*) &output_buffer[0];
  hdr->type = MOB_TYPE;
  hdr->mess = MOB_HR_NEW_G;
  hdr->len = sizeof(mob_new_lbr);
  hdr->reserved = 0;
  data = (mob_new_lbr*) &hdr->next;

  if(flag == MOB_FLAG_LBR_U) {
    data->flags = MOB_FLAG_LBR_U;
    memcpy(&data->addr,lbr,sizeof(uip_ip6addr_t));
    for(i = 0; i < MAX_LBR_BACKUP + 1 ; ++i) {
      if(gws[i].used == MOB_GW_KNOWN) {
        udp_output(&output_buffer[0],hdr->len + MOB_LEN_HDR, &gws[i].addr);
      }
    }
  } else {
    data->flags = flag;
    if(!(mob_type & MOB_TYPE_UPWARD)) {
      data->flags |= MOB_FLAG_LBR_R;
    }
    if (mob_type & MOB_TYPE_STORE) {
      data->flags |= MOB_FLAG_LBR_S;
    }
    memcpy(&data->addr,&myip,sizeof(uip_ip6addr_t));
    udp_output_d(&output_buffer[0], hdr->len + MOB_LEN_HDR, (uip_ipaddr_t *)lbr, port);
  }
}

void
mob_new_6lbr(uip_ipaddr_t *lbr)
{
  int i;
  for(i=0;i<MAX_KNOWN_GATEWAY;++i) {
    if(gws[i].used == MOB_GW_KNOWN &&
        equal(&gws[i].addr.sin6_addr, lbr)) {
      return;
    }
  }
  if(mob_type & MOB_TYPE_UPWARD && gws[0].used == MOB_GW_KNOWN) {
    mob_send_lbr(lbr,MOB_FLAG_LBR_U);
  } else {
    mob_send_lbr(lbr,MOB_FLAG_LBR_Q);
  }
}


void
mob_new_node(uip_ds6_route_t *rep)
{
  memcpy(((uint8_t*)&prefix)+sizeof(uip_lladdr_t),((uint8_t*)&rep->ipaddr)+sizeof(uip_lladdr_t),sizeof(uip_lladdr_t));
  change_local_route("add");
  mob_list_init(rep);
}

void
mob_maj_node(uip_ds6_route_t *rep)
{
  mob_list_maj(rep);
  rep->state.seq = next_out_sequence;
  ++next_out_sequence;

  if(mob_type & MOB_TYPE_UPWARD) {
    if(!ev_is_active(next_send)) {
      printf("Next send activated : %u\n",MOB_SEND_DELAY);
      ev_timer_set(next_send, MOB_SEND_DELAY, 0);
      ev_timer_start(event_loop, next_send);
    } else if(next_send->repeat == MOB_SEND_TIMEOUT) {
     next_send->repeat = MOB_SEND_DELAY;
     ev_timer_again(event_loop,next_send);
    }
  }
  if(mob_type & MOB_TYPE_STORE) {
    if(!ev_is_active(next_cleaner)) {
      printf("Next cleaner activated : %u\n",MOB_SEND_DELAY);
      ev_timer_set(next_cleaner, stimer_remaining(&(rep->state.lifetime)), 0);
      ev_timer_start(event_loop, next_cleaner);
    } else if(next_cleaner->repeat == 0) {
     next_cleaner->repeat = stimer_remaining(&(rep->state.lifetime));
     ev_timer_again(event_loop,next_cleaner);
    }
  }
}

void
mob_lost_node(uip_ds6_route_t *rep)
{
  int i;

  memcpy(((uint8_t*)&prefix)+sizeof(uip_lladdr_t),((uint8_t*)&rep->ipaddr)+sizeof(uip_lladdr_t),sizeof(uip_lladdr_t));
  change_local_route("remove");
  mob_list_remove(rep);

  if(mob_type & MOB_TYPE_UPWARD) {
    for(i=0; i<MAX_DELETE_NIO; ++i) {
      if(!deleted[i].used) {
        deleted[i].used = 1;
        deleted[i].seq = next_out_sequence;
        ++next_out_sequence;
        memcpy(&deleted[i].addr,LLADDR_FROM_IPADDR(&rep->ipaddr),sizeof(uip_lladdr_t));
        return;
      }
    }
    printf("MOB : BUFFER OVERFLOW");
  }
}

void
mob_new_gw(mob_new_lbr *target)
{
  int i;
  struct sockaddr_in6* tgt;
  gw_list *unused_elt = NULL;
  char gw[128];

  toString(&target->addr,gw);

  if(target->flags & MOB_FLAG_LBR_Q) {
    printf("send response 6LBR\n");
    mob_send_lbr(&target->addr,0);
  }

  if(target->flags & MOB_FLAG_LBR_U) {
    printf("send unknown 6LBR\n");
    for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
      if(gws[i].used == MOB_GW_KNOWN) {
        if(equal(&gws[i].addr.sin6_addr,&target->addr)) {
          printf("Known gateway, forget\n");
          return;
        }
      }
      mob_send_lbr(&target->addr, MOB_FLAG_LBR_Q);
    return;
    }
  } else if(target->flags & MOB_FLAG_LBR_R) {
    if(gws[0].used == MOB_GW_KNOWN) {
      if(equal(&gws[0].addr.sin6_addr,&target->addr)) {
        return;
      } else {
        printf("ERROR : Multi-HoAg not supported yet");
        return;
      }
    } else {
      unused_elt = &gws[0];
    }
  } else if(target->flags & MOB_FLAG_LBR_S) {
    for(i = 1; i < MAX_LBR_BACKUP + 1; ++i) {
      if(gws[i].used == MOB_GW_KNOWN) {
        if(equal(&gws[i].addr.sin6_addr,&target->addr)) {
          return;
        }
      } else if(unused_elt == NULL) {
        unused_elt = &gws[i];
      }
    }
  } else {
    for(i = MAX_LBR_BACKUP + 1; i < MAX_KNOWN_GATEWAY; ++i) {
      if(gws[i].used == MOB_GW_KNOWN) {
        if(equal(&gws[i].addr.sin6_addr,&target->addr)) {
          return;
        }
      } else if(unused_elt == NULL) {
        unused_elt = &gws[i];
      }
    }
  }

  if(unused_elt == NULL) {
    printf("NO PLACE FOR NEW 6LBR !!!\n");
    return;
  }

  for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
    if(gws[i].used == MOB_GW_KNOWN) {
      if(equal(&gws[i].addr.sin6_addr,&target->addr)) {
        mob_lbr_evolve(i, unused_elt, target->flags);
        return;
      }
    }
  }

  output_buffer[0]='\n';
  output_buffer[1]='\0';

  tgt = udp_output_d(output_buffer, 1, &target->addr, port);
  if(tgt != NULL) {
    memset(unused_elt,0,sizeof(gw_list));
    unused_elt->used = MOB_GW_KNOWN;
    memcpy(&unused_elt->addr,tgt, sizeof(struct sockaddr_in6));
    udp_output(output_buffer, 1, &unused_elt->addr);

    if(mob_type & MOB_TYPE_APPLY) {
      unused_elt->devnum = tunnelnum++;
      tunnel_create(tuneldev, unused_elt->devnum, &unused_elt->addr);
    }

    mob_lbr_evolve_state(unused_elt, target->flags, 0);

    for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
      if(gws[i].used == MOB_GW_KNOWN && unused_elt != &gws[i]) {
        mob_send_lbr((uip_ip6addr_t *)&gws[i].addr.sin6_addr,MOB_FLAG_LBR_U);
      }
    }
  } else {
    printf("ERROR\n");
  }
}

void
mob_init_lbr(uip_ip6addr_t *lbr)
{
    mob_send_lbr(lbr, MOB_FLAG_LBR_Q);
}

void
clean_routes(uint8_t gw)
{
  uip_ds6_route_t *locroute;

  for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
      locroute++) {
    if(locroute->isused) {
      if(locroute->state.learned_from == RPL_ROUTE_FROM_6LBR
          && (locroute->state.gw = gw)) {
        memcpy(((uint8_t*)&prefix)+sizeof(uip_lladdr_t), &locroute->ipaddr, sizeof(uip_lladdr_t));
        change_route(gw, "del");
        uip_ds6_route_rm(locroute);
      }
    }
  }
}

void
mob_delete_gw(uint8_t gw)
{
  gws[gw].used = 0;
  if(mob_type & MOB_TYPE_STORE) {
    clean_routes(gw);
  }
  if((mob_type & MOB_TYPE_UPWARD) &&
      (gw == 0)) {
    clear_reroute();
    close_tunnel(tuneldev,gws[gw].devnum);
  }
  if(mob_type & MOB_TYPE_APPLY) {
    close_tunnel(tuneldev,gws[gw].devnum);
  }
  if(gw < MAX_LBR_BACKUP) {
    gws[gw].used = MOB_GW_RESERVED;
  }
}

void
mob_close_tunnels(void)
{
  int i;

  if((mob_type & MOB_TYPE_APPLY) ||
      (mob_type & MOB_TYPE_STORE)) {
printf("mobapply\n");
    for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
      if(gws[i].used == MOB_GW_KNOWN) {
        printf("delete : %u\n",i);
        mob_delete_gw(i);
      }
    }
  } else {
    if(gws[0].used == MOB_GW_KNOWN) {
      mob_delete_gw(0);
    }
  }
}

void
receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len)
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
      if(mob_type & MOB_TYPE_STORE) {
        mob_proceed_up((mob_bind_up*)&(buff->next), buff->len, addr, addr_len);
      } else {
        printf("UDP IN : Bad message for 6LBR (Binding Update)\n");
      }
      break;
    case MOB_HR_ACK:
      if(mob_type & MOB_TYPE_UPWARD) {
        mob_incoming_ack((mob_bind_ack*)&buff->next, buff->len, addr);
      } else {
        printf("UDP IN : Bad message for HoAg(Ack)\n");
      }
      break;
    case MOB_HR_NEW_G:
      printf("UDP IN : NEW GW\n");
      mob_new_gw((mob_new_lbr*)&(buff->next));
      break;
    default:
      printf("UDP IN : Bad message (Unknown : %u)",buff->mess);
      break;
 }
}

int
mob_init(uint8_t state, int p, uip_ipaddr_t *pre, uip_ipaddr_t *ip, char* devname, char* tuntty)
{
  int i;
  mob_type = 0;

  for(i=0; i < MAX_LBR_BACKUP + 1; ++i) {
    gws[i].used = MOB_GW_RESERVED;
  }

  memcpy(&prefix,pre,sizeof(uip_ipaddr_t));
  memcpy(&myip,ip,sizeof(uip_ipaddr_t));

  next_out_sequence = 1;
  min_ack_sequence = 0;
  min_out_seq = MOB_LIST_END;
  max_out_seq = MOB_LIST_END;

  port = p;
  tunnelnum = 0;
  strncpy(ttydev,tuntty,MAX_DEVNAME_SIZE-1);
  strncpy(tuneldev,devname,MAX_DEVNAME_SIZE-1);

  if(mob_state_evolve(state)) {
    return -3;
  }

  return 0;
}

void mob_lbr_evolve(uint8_t old_gw, gw_list *new_gw_p, uint8_t new_state)
{
  uip_ds6_route_t *locroute;
  uint8_t new_gw_i = (uint8_t)(new_gw_p - &gws[0]);

  memmove(new_gw_p, &gws[old_gw], sizeof(gw_list));
  memset(&gws[old_gw],0,sizeof(gw_list));
  if(old_gw < MAX_LBR_BACKUP) {
    gws[old_gw].used = MOB_GW_RESERVED;
  }
  if(mob_type & MOB_TYPE_STORE) {
    for(locroute = uip_ds6_routing_table;
        locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
        locroute++) {
      if(locroute->isused
          && locroute->state.learned_from == RPL_ROUTE_FROM_6LBR
          && (locroute->state.gw = old_gw)) {
        locroute->state.gw = new_gw_i;
      }
    }
  }
  if(old_gw == 0) {
    mob_lbr_evolve_state(new_gw_p, new_state, MOB_FLAG_LBR_R | MOB_FLAG_LBR_S);
  } else if(old_gw < MAX_LBR_BACKUP + 1) {
    mob_lbr_evolve_state(new_gw_p, new_state, MOB_FLAG_LBR_S);
  } else {
    mob_lbr_evolve_state(new_gw_p, new_state, 0);
  }
}


void mob_lbr_evolve_state(gw_list *gw, uint8_t new_state, uint8_t old_state)
{
  printf("LBR evolution : %u to %u\n",old_state,new_state);

  if((mob_type & MOB_TYPE_UPWARD)
      && (new_state & MOB_FLAG_LBR_R)
      && (!(old_state & MOB_FLAG_LBR_R))) {
    gw->devnum = tunnelnum++;
    tunnel_create(tuneldev, gw->devnum, &gw->addr);
    create_reroute(tuneldev, gw->devnum);
  }
  printf("previous seq : %u (%u)\n",gw->sequence_out,min_out_seq);
  if((new_state ^ old_state) & MOB_FLAG_LBR_S) {
    if(min_out_seq != MOB_LIST_END) {
      min_ack_sequence = (gw->sequence_out = uip_ds6_routing_table[min_out_seq].state.seq - 1);
    } else {
      gw->sequence_out = min_ack_sequence;
    }
 }
  printf("New seq : %u \n",gw->sequence_out);
  if((new_state & MOB_FLAG_LBR_S)
      && (mob_type & MOB_TYPE_UPWARD)) {
    if(!ev_is_active(next_send)) {
      printf("Next send activated : %u\n",MOB_SEND_DELAY);
      ev_timer_set(next_send, MOB_SEND_DELAY, 0);
      ev_timer_start(event_loop, next_send);
    }
  }
}

/*
*/

int
mob_state_evolve(uint8_t new_state)
{
  int i;
  uip_ds6_route_t *locroute;

  if((new_state & MOB_TYPE_APPLY)
      && (new_state & MOB_TYPE_UPWARD)) {
    printf("ERROR : MOB_TYPE_APPLY and MOB_TYPE_UPWARD uncompatibles\n");
    return 3;
  }
  if((new_state & MOB_TYPE_APPLY)
      && (!(new_state & MOB_TYPE_STORE))) {
    printf("ERROR : MOB_TYPE_APPLY and !MOB_TYPE_STORE uncompatibles\n");
    return 3;
  }

  if(mob_type & MOB_TYPE_APPLY) {
    if(!(new_state& MOB_TYPE_APPLY)) {
      for(i=1; i < MAX_KNOWN_GATEWAY; ++i) {
        if(gws[i].used == MOB_GW_KNOWN) {
          close_tunnel(tuneldev,gws[i].devnum);
          gws[i].devnum = 0;
        }
      }
    }
  } else if(new_state& MOB_TYPE_APPLY) {
    for(i=1; i < MAX_KNOWN_GATEWAY; ++i) {
      if(gws[i].used == MOB_GW_KNOWN) {
        gws[i].devnum = tunnelnum++;
        tunnel_create(tuneldev, gws[i].devnum, &gws[i].addr);
      }
    }
    for(locroute = uip_ds6_routing_table;
        locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
        locroute++) {
      if(locroute->isused
          && locroute->state.learned_from == RPL_ROUTE_FROM_6LBR) {
        memcpy(((uint8_t*)&prefix)+sizeof(uip_lladdr_t), &locroute->ipaddr, sizeof(uip_lladdr_t));
        change_route(locroute->state.gw, "add");
      }
    }
  }

  if(mob_type & MOB_TYPE_STORE) {
    if (!(new_state & MOB_TYPE_STORE)) {
      for(locroute = uip_ds6_routing_table;
          locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
          locroute++) {
        if(locroute->isused
            && locroute->state.learned_from == RPL_ROUTE_FROM_6LBR) {
          uip_ds6_route_rm(locroute);
        }
      }
      ev_timer_stop(event_loop,next_cleaner);
     free(next_cleaner);
      next_cleaner = NULL;
    }
  } else if (new_state & MOB_TYPE_STORE) {
    if((next_cleaner = (ev_timer*) malloc(sizeof(ev_timer))) == NULL) {
      return 2;
     }
    ev_init(next_cleaner, mob_clean_entry);
  }

  if(mob_type & MOB_TYPE_UPWARD) {
    if(!(new_state& MOB_TYPE_UPWARD)) {
      if(gws[0].used == MOB_GW_KNOWN) {
        clear_reroute();
      }
      ev_timer_stop(event_loop,next_send);
      free(next_send);
      next_send = NULL;
    }
  } else if(new_state& MOB_TYPE_UPWARD) {
    if((next_send = (ev_timer*) malloc(sizeof(ev_timer))) == NULL) {
      return 2;
    }
    ev_init(next_send,mob_send_message);
  }

  if(mob_type ^ new_state) {
    mob_type = new_state;
    for(i = 0; i < MAX_KNOWN_GATEWAY; ++i) {
      if(gws[i].used == MOB_GW_KNOWN) {
        mob_send_lbr((uip_ipaddr_t*)&gws[i].addr.sin6_addr,0);
      }
    }
  }

  mob_type = new_state;
  return 0;
}

void
mob_list_add_end(uip_ds6_route_t *rep)
{
  uint16_t temp = (uint16_t) (rep - uip_ds6_routing_table);

  if(rep->state.learned_from == RPL_ROUTE_FROM_6LBR) {
    if(max_in_seq == MOB_LIST_END) {
      min_in_seq = temp;
      rep->state.prev_seq = MOB_LIST_END;
    } else {
      uip_ds6_routing_table[max_in_seq].state.next_seq = temp;
      rep->state.prev_seq = max_in_seq;
    }
    max_in_seq = temp;
  } else {
    if(max_out_seq == MOB_LIST_END) {
      min_out_seq = temp;
      rep->state.prev_seq = MOB_LIST_END;
    } else {
      uip_ds6_routing_table[max_out_seq].state.next_seq = temp;
      rep->state.prev_seq = max_out_seq;
    }
    max_out_seq = temp;
  }
  rep->state.next_seq = MOB_LIST_END;
}

void
mob_list_remove(uip_ds6_route_t *rep)
{
  if(rep->state.prev_seq == MOB_LIST_END) {
    if(rep->state.next_seq == MOB_LIST_END) {
      if(rep->state.learned_from == RPL_ROUTE_FROM_6LBR) {
        min_in_seq = MOB_LIST_END;
        max_in_seq = MOB_LIST_END;
      } else {
        min_out_seq = MOB_LIST_END;
        max_out_seq = MOB_LIST_END;
      }
    } else {
      if(rep->state.learned_from == RPL_ROUTE_FROM_6LBR) {
        min_in_seq = rep->state.next_seq;
      } else {
        min_out_seq = rep->state.next_seq;
      }
      uip_ds6_routing_table[rep->state.next_seq].state.prev_seq = MOB_LIST_END;
    }
  } else {
    if(rep->state.next_seq == MOB_LIST_END) {
      if(rep->state.learned_from == RPL_ROUTE_FROM_6LBR) {
        max_in_seq = rep->state.prev_seq;
      } else {
        max_out_seq = rep->state.prev_seq;
      }
      uip_ds6_routing_table[rep->state.prev_seq].state.next_seq = MOB_LIST_END;
    } else {
      uip_ds6_routing_table[rep->state.prev_seq].state.next_seq = rep->state.next_seq;
      uip_ds6_routing_table[rep->state.next_seq].state.prev_seq = rep->state.prev_seq;
    }
  }
}

void
mob_update_min_ack(uint16_t new_ack)
{
  int i;
  uint16_t temp = next_out_sequence;
  if(new_ack == min_ack_sequence) {
    for(i=0; i < MAX_KNOWN_GATEWAY; ++i) {
      if(gws[i].used == MOB_GW_KNOWN
          && UINT_LESS_THAN(gws[i].sequence_out,temp)) {
        temp = gws[i].sequence_out;
      }
    }
  }
  if(temp != next_out_sequence) {
    min_ack_sequence = temp;
  } else {
    min_ack_sequence = next_out_sequence -1;
  }
}

void
mob_list_init(uip_ds6_route_t *rep)
{
  rep->state.prev_seq = MOB_LIST_INIT;
  rep->state.next_seq = MOB_LIST_INIT;
}

void
mob_list_maj(uip_ds6_route_t *rep)
{
  if(rep->state.prev_seq != MOB_LIST_INIT) {
    mob_list_remove(rep);
  }
  mob_list_add_end(rep);
}


void
mob_clean_entry(struct ev_loop *loop, struct ev_timer *w, int revents)
{
  uint16_t pos,next;
  unsigned long temp, next_timer;
  pos = min_out_seq;
  next = min_out_seq;
  next_timer = 0;

  while(next != MOB_LIST_END) {
    pos = next;
    next = uip_ds6_routing_table[pos].state.next_seq;
    if(UIP_DS6_ROUTE_STATE_CLEAN(&(uip_ds6_routing_table[pos].state))) {
      memcpy(((uint8_t*)&prefix)+sizeof(uip_lladdr_t), &(uip_ds6_routing_table[pos].ipaddr), sizeof(uip_lladdr_t));
      change_route(uip_ds6_routing_table[pos].state.gw, "del");
      uip_ds6_route_rm(&(uip_ds6_routing_table[pos]));
      mob_lost_node(&uip_ds6_routing_table[pos]);
    } else {
      if((temp = stimer_remaining(&(uip_ds6_routing_table[pos].state.lifetime))) < next_timer) {
        next_timer = temp;
      }
    }
  }
  if(next_timer >= 0) {
    w->repeat = next_timer;
  }
}

void
mob_check_all_entry(void)
{
  uint16_t pos,next;
  pos = min_out_seq;
  next = min_out_seq;

  while(next != MOB_LIST_END) {
    pos = next;
    next = uip_ds6_routing_table[pos].state.next_seq;
    if(UIP_DS6_ROUTE_STATE_CLEAN(&(uip_ds6_routing_table[pos].state))) {
      uip_ds6_route_rm(&(uip_ds6_routing_table[pos]));
      mob_lost_node(&uip_ds6_routing_table[pos]);
    }
  }
}

