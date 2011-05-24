/*
 * Copyright (c) 2009, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

#include "uip6.h"
#include "tcpip.h"
#include "uip-ds6.h"
#include "rpl/rpl-private.h"

#define DEBUG DEBUG_NONE
#include "uip-debug.h"

#include <limits.h>
#include <string.h>


/************************************************************************/
extern uip_ds6_route_t uip_ds6_routing_table[UIP_DS6_ROUTE_NB];

#define UIP_IP_BUF                          ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_EXT_BUF                        ((struct uip_ext_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_HBHO_BUF                      ((struct uip_hbho_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_HBHO_NEXT_BUF                  ((struct uip_ext_hdr *)&uip_buf[uip_l2_l3_hdr_len + RPL_OP_BY_OP_LEN])
#define UIP_EXT_HDR_OPT_BUF            ((struct uip_ext_hdr_opt *)&uip_buf[uip_l2_l3_hdr_len + uip_ext_opt_offset])
#define UIP_EXT_HDR_OPT_PADN_BUF  ((struct uip_ext_hdr_opt_padn *)&uip_buf[uip_l2_l3_hdr_len + uip_ext_opt_offset])
#define UIP_EXT_HDR_OPT_RPL_BUF    ((struct uip_ext_hdr_opt_rpl *)&uip_buf[uip_l2_l3_hdr_len + uip_ext_opt_offset])

/************************************************************************/
void
rpl_remove_routes(rpl_dag_t *dag)
{
  int i;

  for(i = 0; i < UIP_DS6_ROUTE_NB; i++) {
    if(uip_ds6_routing_table[i].state.dag == dag) {
      uip_ds6_route_rm(&uip_ds6_routing_table[i]);
    }
  }
}
/************************************************************************/
void
rpl_remove_routes_by_nexthop(uip_ipaddr_t *nexthop, rpl_dag_t *dag)
{
  uip_ds6_route_t *locroute;

  for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
      locroute++) {
    if(locroute->isused
        && uip_ipaddr_cmp(&locroute->nexthop, nexthop)
        && locroute->state.dag == dag) {
      locroute->isused = 0;
    }
  }
  ANNOTATE("#L %u 0\n",nexthop->u8[sizeof(uip_ipaddr_t) - 1]);
}
/************************************************************************/
uip_ds6_route_t *
rpl_add_route(rpl_dag_t *dag, uip_ipaddr_t *prefix, int prefix_len,
              uip_ipaddr_t *next_hop, uint8_t learned_from)
{
  uip_ds6_route_t *rep;

  rep = uip_ds6_route_lookup(prefix);
  if(rep == NULL) {
    if((rep = uip_ds6_route_add(prefix, prefix_len, next_hop, 0)) == NULL) {
      PRINTF("RPL: No space for more route entries\n");
      return NULL;
    }
  } else {
    PRINTF("RPL: Updated the next hop for prefix ");
    PRINT6ADDR(prefix);
    PRINTF(" to ");
    PRINT6ADDR(next_hop);
    PRINTF("\n");
    uip_ipaddr_copy(&rep->nexthop, next_hop);
  }
  rep->state.dag = dag;
  stimestamp_set(&rep->state.lifetime, dag->instance->default_lifetime * dag->instance->lifetime_unit);
  rep->state.learned_from = learned_from;

  PRINTF("RPL: Added a route to ");
  PRINT6ADDR(prefix);
  PRINTF("/%d via ", prefix_len);
  PRINT6ADDR(next_hop);
  PRINTF("\n");

  return rep;
}
/************************************************************************/
/*
static void
rpl_link_neighbor_callback(const rimeaddr_t *addr, int known, int etx)
{
  uip_ipaddr_t ipaddr;
  rpl_parent_t *parent;
  rpl_instance_t *instance;
  rpl_instance_t *end;

  uip_ip6addr(&ipaddr, 0xfe80, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, (uip_lladdr_t *)addr);
  PRINTF("RPL: Neighbor ");
  PRINT6ADDR(&ipaddr);
  PRINTF(" is %sknown. ETX = %u\n", known ? "" : "no longer ", NEIGHBOR_INFO_FIX2ETX(etx));

  for( instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES; instance < end; ++instance) {
    if ( instance->used == 1 ) {
      parent = rpl_find_parent_any_dag(instance, &ipaddr);
      if(!(parent == NULL)) {
        parent->link_metric = etx;
        parent->updated = 1;
        rpl_update_periodic_timer();

        if(instance->of->parent_state_callback != NULL) {
          instance->of->parent_state_callback(parent, known, etx);
        }
        if(!known) {
          PRINTF("RPL: Removing parent ");
          PRINT6ADDR(&parent->addr);
          PRINTF(" in instance %u because of bad connectivity (ETX %d)\n", instance->instance_id, etx);
          parent->rank = INFINITE_RANK;
        }
      }
    }
  }

  if (!known) {
    PRINTF("RPL: Deleting routes installed by DAOs received from ");
    PRINT6ADDR(&ipaddr);
    PRINTF("\n");
    uip_ds6_route_rm_by_nexthop(&ipaddr);
  }
}
*/
/************************************************************************/
void
rpl_ipv6_neighbor_callback(uip_ds6_nbr_t *nbr)
{
  rpl_parent_t *p;
  rpl_instance_t *instance;
  rpl_instance_t *end;

  if(!nbr->isused) {
    PRINTF("RPL: Removing neighbor ");
    PRINT6ADDR(&nbr->ipaddr);
    PRINTF("\n");
    for( instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES; instance < end; ++instance) {
      if ( instance->used == 1 ) {
        p = rpl_find_parent_any_dag(instance, &nbr->ipaddr);
        if(p != NULL) {
          p->rank = INFINITE_RANK;
          /* Trigger DAG rank recalculation. */
          p->updated = 1;
          rpl_update_periodic_timer();
        }
      }
    }
  }
}
/************************************************************************/
void
rpl_init(void)
{
  uip_ipaddr_t rplmaddr;
  PRINTF("RPL started\n");
  default_instance=NULL;

  rpl_reset_periodic_timer();

  /* add rpl multicast address */
  uip_create_linklocal_rplnodes_mcast(&rplmaddr);
  uip_ds6_maddr_add(&rplmaddr);

}
/************************************************************************/
int
rpl_verify_header(int uip_ext_opt_offset)
{
  rpl_instance_t *instance;

  if (UIP_EXT_HDR_OPT_RPL_BUF->opt_len != RPL_HDR_OPT_LEN) {
    PRINTF("RPL: Bad header option (wrong length) !\n");
    return 1;
  }

  if (UIP_EXT_HDR_OPT_RPL_BUF->flags&RPL_HDR_OPT_FWD_ERR) {
    PRINTF("RPL: Forward error !\n");
    /* We should try to repair it, not implemented for the moment */
    return 2;
  }

  instance = rpl_get_instance(UIP_EXT_HDR_OPT_RPL_BUF->instance);

  if (instance == NULL) {
    PRINTF("RPL : Unknown instance : %u\n",UIP_EXT_HDR_OPT_RPL_BUF->instance);
    return 1;
  }

  if (!instance->current_dag->joined) {
    PRINTF("RPL : No dag in the instance\n");
    return 1;
  }

  if (UIP_EXT_HDR_OPT_RPL_BUF->flags&RPL_HDR_OPT_DOWN) {
    PRINTF("RPL: Packet going down :\n");
    if (UIP_EXT_HDR_OPT_RPL_BUF->senderrank>instance->current_dag->rank) {
      PRINTF("RPL: Loop detected : Sender rank > our rank\n");
      if (UIP_EXT_HDR_OPT_RPL_BUF->flags&RPL_HDR_OPT_RANK_ERR) {
        PRINTF("RPL: Loop detected !\n");
        /* We should try to repair it, not implemented for the moment */
        return 3;
      }
      PRINTF("RPL: Single error tolerated\n");
      UIP_EXT_HDR_OPT_RPL_BUF->flags|=RPL_HDR_OPT_RANK_ERR;
      return 0;
    }
  } else {
    PRINTF("RPL: Packet going up :");
    if (UIP_EXT_HDR_OPT_RPL_BUF->senderrank<instance->current_dag->rank) {
      PRINTF("RPL: Rank error : Sender rank < our rank\n");
      if (UIP_EXT_HDR_OPT_RPL_BUF->flags&RPL_HDR_OPT_RANK_ERR) {
        PRINTF("RPL: Loop detected !\n");
        /* We should try to repair it, not implemented for the moment */
        return 3;
      }
      PRINTF("RPL: Single error tolerated\n");
      UIP_EXT_HDR_OPT_RPL_BUF->flags|=RPL_HDR_OPT_RANK_ERR;
      return 0;
    }
  }
    PRINTF("RPL: rank Ok\n");
  return 0;
}
/************************************************************************/
void
rpl_update_header_empty(void)
{
  rpl_instance_t *instance;
  int uip_ext_opt_offset;
  int last_uip_ext_len;
  u8_t temp_len;

  last_uip_ext_len=uip_ext_len;
  uip_ext_len=0;
  uip_ext_opt_offset = 2;

  PRINTF("RPL: Verifying the presence of the RPL header option\n");
  switch(UIP_IP_BUF->proto){
    case UIP_PROTO_HBHO:
      if (UIP_HBHO_BUF->len != RPL_OP_BY_OP_LEN - 8) {
        PRINTF("RPL: Non RPL Hop-by-hop options support not implemented\n");
        uip_ext_len=last_uip_ext_len;
        return;
      }
      instance = rpl_get_instance(UIP_EXT_HDR_OPT_RPL_BUF->instance);
      if(instance == NULL || (!instance->used) || (!instance->current_dag->joined) ) {
        PRINTF("Unable to add RPL hop-by-hop extension header : incorrect instance\n");
        return;
      }
      break;
    default:
      PRINTF("RPL: No Hop-by-Hop Option found, creating it\n");
      if(uip_len + RPL_OP_BY_OP_LEN >UIP_LINK_MTU) {
        PRINTF("RPL: Packet too long : impossible to add rpl Hop-by-Hop option\n");
        uip_ext_len=last_uip_ext_len;
        return;
      }
      memmove(UIP_HBHO_NEXT_BUF,UIP_EXT_BUF,uip_len-UIP_IPH_LEN);
      memset(UIP_HBHO_BUF,0,RPL_OP_BY_OP_LEN);
      UIP_HBHO_BUF->next=UIP_IP_BUF->proto;
      UIP_IP_BUF->proto=UIP_PROTO_HBHO;
      UIP_HBHO_BUF->len=RPL_OP_BY_OP_LEN - 8;
      UIP_EXT_HDR_OPT_RPL_BUF->opt_type=UIP_EXT_HDR_OPT_RPL;
      UIP_EXT_HDR_OPT_RPL_BUF->opt_len=RPL_HDR_OPT_LEN;
      UIP_EXT_HDR_OPT_RPL_BUF->flags=0;
      UIP_EXT_HDR_OPT_RPL_BUF->instance=0;
      UIP_EXT_HDR_OPT_RPL_BUF->senderrank=0;
      uip_len+=RPL_OP_BY_OP_LEN;
      temp_len=UIP_IP_BUF->len[1];
      UIP_IP_BUF->len[1]+=(UIP_HBHO_BUF->len + 8);
      if (UIP_IP_BUF->len[1]<temp_len) {
        UIP_IP_BUF->len[0]+=1;
      }
      uip_ext_len=last_uip_ext_len+RPL_OP_BY_OP_LEN;
      return;
  }
  switch (UIP_EXT_HDR_OPT_BUF->type) {
    case UIP_EXT_HDR_OPT_RPL:
      PRINTF("RPL: Updating RPL option\n");
      UIP_EXT_HDR_OPT_RPL_BUF->senderrank=instance->current_dag->rank;
      uip_ext_len=last_uip_ext_len;
      return;
    default:
      PRINTF("RPL: Multi Hop-by-hop options not implemented\n");
      uip_ext_len=last_uip_ext_len;
      return;
  }
}
/************************************************************************/
int
rpl_update_header_final(uip_ipaddr_t *addr)
{
  int uip_ext_opt_offset;
  int last_uip_ext_len;

  last_uip_ext_len=uip_ext_len;
  uip_ext_len=0;
  uip_ext_opt_offset = 2;
  rpl_parent_t *parent;

  if (UIP_IP_BUF->proto == UIP_PROTO_HBHO){
    if (UIP_HBHO_BUF->len != RPL_OP_BY_OP_LEN - 8) {
      PRINTF("RPL: Non RPL Hop-by-hop options support not implemented\n");
      uip_ext_len=last_uip_ext_len;
      return 0;
    }
    if (UIP_EXT_HDR_OPT_BUF->type == UIP_EXT_HDR_OPT_RPL) {
      if (UIP_EXT_HDR_OPT_RPL_BUF->senderrank==0) {
        PRINTF("RPL: Updating RPL option\n");
        if(default_instance == NULL || (!default_instance->used) || (!default_instance->current_dag->joined) ) {
          PRINTF("Unable to add RPL hop-by-hop extension header : incorrect default instance\n");
          return 1;
        }
        parent=rpl_find_parent(default_instance->current_dag,addr);
        if (parent == NULL || (parent != parent->dag->preferred_parent)) {
          UIP_EXT_HDR_OPT_RPL_BUF->flags=RPL_HDR_OPT_DOWN;
        }
        UIP_EXT_HDR_OPT_RPL_BUF->instance=default_instance->instance_id;
        UIP_EXT_HDR_OPT_RPL_BUF->senderrank=default_instance->current_dag->rank;
        uip_ext_len=last_uip_ext_len;
      }
    }
  }
  return 0;
}
/************************************************************************/
int
rpl_add_header(rpl_instance_t *instance,int down)
{
  int uip_ext_opt_offset;
  int last_uip_ext_len;
  u8_t temp_len;

  last_uip_ext_len = uip_ext_len;
  uip_ext_len = 0;
  uip_ext_opt_offset = 2;

  if(instance == NULL || (!instance->used) || (!instance->current_dag->joined) ) {
    PRINTF("Unable to add RPL hop-by-hop extension header : incorrect instance\n");
    return 0;
  }

  PRINTF("RPL: Verifying the presence of the RPL header option\n");
  switch(UIP_IP_BUF->proto){
    case UIP_PROTO_HBHO:
      if (UIP_HBHO_BUF->len != RPL_OP_BY_OP_LEN - 8) {
        PRINTF("RPL: Non RPL Hop-by-hop options support not implemented\n");
        uip_ext_len=last_uip_ext_len;
        return 0;
      }
      break;
    default:
      PRINTF("RPL: No Hop-by-Hop Option found, creating it\n");
      if(uip_len + RPL_OP_BY_OP_LEN >UIP_LINK_MTU) {
        PRINTF("RPL: Packet too long : impossible to add rpl Hop-by-Hop option\n");
        uip_ext_len=last_uip_ext_len;
        return 0;
      }
      memmove(UIP_HBHO_NEXT_BUF,UIP_EXT_BUF,uip_len-UIP_IPH_LEN);
      memset(UIP_HBHO_BUF,0,RPL_OP_BY_OP_LEN);
      UIP_HBHO_BUF->next=UIP_IP_BUF->proto;
      UIP_IP_BUF->proto=UIP_PROTO_HBHO;
      UIP_HBHO_BUF->len=RPL_OP_BY_OP_LEN - 8;
      UIP_EXT_HDR_OPT_RPL_BUF->opt_type=UIP_EXT_HDR_OPT_RPL;
      UIP_EXT_HDR_OPT_RPL_BUF->opt_len=RPL_HDR_OPT_LEN;
      UIP_EXT_HDR_OPT_RPL_BUF->flags=RPL_HDR_OPT_DOWN&(down<<RPL_HDR_OPT_DOWN_SHIFT);
      UIP_EXT_HDR_OPT_RPL_BUF->instance=instance->instance_id;
      UIP_EXT_HDR_OPT_RPL_BUF->senderrank=instance->current_dag->rank;
      uip_len+=RPL_OP_BY_OP_LEN;
      temp_len=UIP_IP_BUF->len[1];
      UIP_IP_BUF->len[1]+=(UIP_HBHO_BUF->len + 8);
      if (UIP_IP_BUF->len[1]<temp_len) {
        UIP_IP_BUF->len[0]+=1;
      }
      uip_ext_len=last_uip_ext_len+RPL_OP_BY_OP_LEN;
      return 1;
  }
  switch (UIP_EXT_HDR_OPT_BUF->type) {
    case UIP_EXT_HDR_OPT_RPL:
      PRINTF("RPL: Updating RPL option\n");
      UIP_EXT_HDR_OPT_RPL_BUF->opt_type=UIP_EXT_HDR_OPT_RPL;
      UIP_EXT_HDR_OPT_RPL_BUF->opt_len=RPL_HDR_OPT_LEN;
      UIP_EXT_HDR_OPT_RPL_BUF->flags=RPL_HDR_OPT_DOWN&(down<<RPL_HDR_OPT_DOWN_SHIFT);
      UIP_EXT_HDR_OPT_RPL_BUF->instance=instance->instance_id;
      UIP_EXT_HDR_OPT_RPL_BUF->senderrank=instance->current_dag->rank;
      uip_ext_len=last_uip_ext_len;
      return 1;
    default:
      PRINTF("RPL: Multi Hop-by-hop options not implemented\n");
      uip_ext_len=last_uip_ext_len;
      return 0;
  }
}
/************************************************************************/
int
rpl_add_header_root(void)
{
  return rpl_add_header(default_instance,1);
}
/************************************************************************/
void
rpl_remove_header()
{
  int last_uip_ext_len;
  u8_t temp_len;

  last_uip_ext_len=uip_ext_len;
  uip_ext_len=0;

  PRINTF("RPL: Verifying the presence of the RPL header option\n");
  switch(UIP_IP_BUF->proto){
    case UIP_PROTO_HBHO:
      PRINTF("RPL: Removing the present RPL header option\n");
      UIP_IP_BUF->proto=UIP_HBHO_BUF->next;
      temp_len=UIP_IP_BUF->len[1];
      uip_len-=UIP_HBHO_BUF->len + 8;
      UIP_IP_BUF->len[1]-=(UIP_HBHO_BUF->len + 8);
      if (UIP_IP_BUF->len[1]>temp_len) {
        UIP_IP_BUF->len[0]-=1;
      }
      memmove(UIP_EXT_BUF, UIP_HBHO_NEXT_BUF, uip_len - UIP_IPH_LEN);
      break;
    default:
      PRINTF("RPL: No Hop-by-Hop Option found\n");
  }
}
/************************************************************************/
u8_t
rpl_invert_header()
{
  u8_t uip_ext_opt_offset;
  u8_t last_uip_ext_len;

  last_uip_ext_len=uip_ext_len;
  uip_ext_len=0;
  uip_ext_opt_offset = 2;

  PRINTF("RPL: Verifying the presence of the RPL header option\n");
  switch(UIP_IP_BUF->proto){
    case UIP_PROTO_HBHO:
      break;
    default:
      PRINTF("RPL: No Hop-by-Hop Option found\n");
      uip_ext_len=last_uip_ext_len;
      return 0;
  }
  switch (UIP_EXT_HDR_OPT_BUF->type) {
    case UIP_EXT_HDR_OPT_RPL:
      PRINTF("RPL: Updating RPL option (Inverting Up<->Down)\n");
      UIP_EXT_HDR_OPT_RPL_BUF->flags&=RPL_HDR_OPT_DOWN;
      UIP_EXT_HDR_OPT_RPL_BUF->flags^=RPL_HDR_OPT_DOWN;
      UIP_EXT_HDR_OPT_RPL_BUF->senderrank=rpl_get_instance(UIP_EXT_HDR_OPT_RPL_BUF->instance)->current_dag->rank;
      uip_ext_len=last_uip_ext_len;
      return RPL_OP_BY_OP_LEN;
    default:
      PRINTF("RPL: Multi Hop-by-hop options not implemented\n");
      uip_ext_len=last_uip_ext_len;
      return 0;
  }
}
/************************************************************************/
