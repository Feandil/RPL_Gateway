/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
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
 */
#include <string.h>
#include <stdlib.h>
#include "lib/random.h"
#include "uip-ds6.h"
#include "sys/event.h"

#define DEBUG 1
#include "uip-debug.h"

/** \name "DS6" Data structures */
/** @{ */
uip_ds6_netif_t uip_ds6_if;                                       /** \brief The single interface */
uip_ds6_prefix_t uip_ds6_prefix_list[UIP_DS6_PREFIX_NB];          /** \brief Prefix list */
uip_ds6_route_t uip_ds6_routing_table[UIP_DS6_ROUTE_NB];          /** \brief Routing table */

/** @} */

/* "full" (as opposed to pointer) ip address used in this file,  */
static uip_ipaddr_t loc_fipaddr;

/* Pointers used in this file */
static uip_ds6_addr_t *locaddr;
static uip_ds6_maddr_t *locmaddr;
static uip_ds6_aaddr_t *locaaddr;
static uip_ds6_prefix_t *locprefix;
static uip_ds6_route_t *locroute;

/* uint8_t used in this file */
static uint8_t loc_loop_state;

void mob_lost_node(uip_ds6_route_t *rep);

/*---------------------------------------------------------------------------*/
void
uip_ds6_init(void)
{
  PRINTF("Init of IPv6 data structures\n");
  PRINTF("%u neighbors\n%u default routers\n%u prefixes\n%u routes\n%u unicast addresses\n%u multicast addresses\n%u anycast addresses\n",
     UIP_DS6_NBR_NB, UIP_DS6_DEFRT_NB, UIP_DS6_PREFIX_NB, UIP_DS6_ROUTE_NB,
     UIP_DS6_ADDR_NB, UIP_DS6_MADDR_NB, UIP_DS6_AADDR_NB);
  memset(uip_ds6_prefix_list, 0, sizeof(uip_ds6_prefix_list));
  memset(&uip_ds6_if, 0, sizeof(uip_ds6_if));
  memset(uip_ds6_routing_table, 0, sizeof(uip_ds6_routing_table));

  /* Set interface parameters */
  uip_ds6_if.link_mtu = UIP_LINK_MTU;
  uip_ds6_if.cur_hop_limit = UIP_TTL;
  uip_ds6_if.maxdadns = 0;

  /* Create link local address, prefix, multicast addresses, anycast addresses */
  uip_create_linklocal_prefix(&loc_fipaddr);
  uip_ds6_prefix_add(&loc_fipaddr, UIP_DEFAULT_PREFIX_LEN, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&loc_fipaddr, &uip_lladdr);
  uip_ds6_addr_add(&loc_fipaddr, 0, ADDR_AUTOCONF);

  uip_create_linklocal_allnodes_mcast(&loc_fipaddr);
  uip_ds6_maddr_add(&loc_fipaddr);
  uip_create_linklocal_allrouters_mcast(&loc_fipaddr);
  uip_ds6_maddr_add(&loc_fipaddr);
  PRINTF("DS: init set timer to %u\n",UIP_DS6_PERIOD);
  return;
}

uint8_t
uip_ds6_list_loop(uip_ds6_element_t *list, uint8_t size,
                  uint16_t elementsize, uip_ipaddr_t *ipaddr,
                  uint8_t ipaddrlen, uip_ds6_element_t **out_element)
{
  uip_ds6_element_t *element;

  *out_element = NULL;

  for(element = list;
      element <
      (uip_ds6_element_t *)((uint8_t *)list + (size * elementsize));
      element = (uip_ds6_element_t *)((uint8_t *)element + elementsize)) {
    if(element->isused) {
      if(uip_ipaddr_prefixcmp(&element->ipaddr, ipaddr, ipaddrlen)) {
        *out_element = element;
        return FOUND;
      }
    } else {
      *out_element = element;
    }
  }

  return *out_element != NULL ? FREESPACE : NOSPACE;
}

uip_ds6_prefix_t *
uip_ds6_prefix_add(uip_ipaddr_t *ipaddr, uint8_t ipaddrlen,
                   uint8_t advertise, uint8_t flags, unsigned long vtime,
                   unsigned long ptime)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_prefix_list, UIP_DS6_PREFIX_NB,
      sizeof(uip_ds6_prefix_t), ipaddr, ipaddrlen,
      (uip_ds6_element_t **)&locprefix) == FREESPACE) {
    locprefix->isused = 1;
    uip_ipaddr_copy(&locprefix->ipaddr, ipaddr);
    locprefix->length = ipaddrlen;
    locprefix->advertise = advertise;
    locprefix->l_a_reserved = flags;
    locprefix->vlifetime = vtime;
    locprefix->plifetime = ptime;
    PRINTF("Adding prefix ");
    PRINT6ADDR(&locprefix->ipaddr);
    PRINTF("length %u, flags %x, Valid lifetime %lx, Preffered lifetime %lx\n",
       ipaddrlen, flags, vtime, ptime);
    return locprefix;
  } else {
    PRINTF("No more space in Prefix list\n");
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_prefix_rm(uip_ds6_prefix_t * prefix)
{
  if(prefix != NULL) {
    prefix->isused = 0;
  }
  return;
}
/*---------------------------------------------------------------------------*/
uip_ds6_prefix_t *
uip_ds6_prefix_lookup(uip_ipaddr_t *ipaddr, uint8_t ipaddrlen)
{
  if(uip_ds6_list_loop((uip_ds6_element_t *)uip_ds6_prefix_list,
		       UIP_DS6_PREFIX_NB, sizeof(uip_ds6_prefix_t),
		       ipaddr, ipaddrlen,
		       (uip_ds6_element_t **)&locprefix) == FOUND) {
    return locprefix;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uint8_t
uip_ds6_is_addr_onlink(uip_ipaddr_t *ipaddr)
{
  for(locprefix = uip_ds6_prefix_list;
      locprefix < uip_ds6_prefix_list + UIP_DS6_PREFIX_NB; locprefix++) {
    if(locprefix->isused &&
       uip_ipaddr_prefixcmp(&locprefix->ipaddr, ipaddr, locprefix->length)) {
      return 1;
    }
  }
  return 0;
}

/*---------------------------------------------------------------------------*/
inline void
uip_ds6_addr_clean(void)
{
  uip_ds6_addr_t *element;

  for(element = uip_ds6_if.addr_list;
      element < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB;
      ++element) {
    if(!element->isinfinite
        && stimestamp_expired(&(element->vlifetime))) {
      uip_ds6_addr_rm(element);
      locaddr = element;
      loc_loop_state = FREESPACE;
      return;
    }
  }
}

/*---------------------------------------------------------------------------*/
uip_ds6_addr_t *
uip_ds6_addr_add(uip_ipaddr_t *ipaddr, unsigned long vlifetime, uint8_t type)
{
  loc_loop_state = uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_if.addr_list, UIP_DS6_ADDR_NB,
      sizeof(uip_ds6_addr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locaddr);

  if(loc_loop_state == NOSPACE) {
    uip_ds6_addr_clean();
   }

  if(loc_loop_state == FREESPACE) {
    locaddr->isused = 1;
    uip_ipaddr_copy(&locaddr->ipaddr, ipaddr);
    locaddr->type = type;
    if(vlifetime == 0) {
      locaddr->isinfinite = 1;
    } else {
      locaddr->isinfinite = 0;
      stimestamp_set(&(locaddr->vlifetime), vlifetime);
    }
    locaddr->state = ADDR_PREFERRED;
    uip_create_solicited_node(ipaddr, &loc_fipaddr);
    uip_ds6_maddr_add(&loc_fipaddr);
    return locaddr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_addr_rm(uip_ds6_addr_t *addr)
{
  if(addr != NULL) {
    uip_create_solicited_node(&addr->ipaddr, &loc_fipaddr);
    if(((locmaddr = uip_ds6_maddr_lookup(&loc_fipaddr)) != NULL)
         && (uip_ds6_maddr_solicited_node_verify(locmaddr) == NULL)) {
      uip_ds6_maddr_rm(locmaddr);
    }
    addr->isused = 0;
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_addr_t *
uip_ds6_addr_lookup(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_if.addr_list, UIP_DS6_ADDR_NB,
      sizeof(uip_ds6_addr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locaddr) == FOUND) {
    if((!locaddr->isinfinite) && (stimestamp_expired(&locaddr->vlifetime))) {
      uip_ds6_addr_rm(locaddr);
      return NULL;
    }
    return locaddr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
/*
 * get a link local address -
 * state = -1 => any address is ok. Otherwise state = desired state of addr.
 * (TENTATIVE, PREFERRED, DEPRECATED)
 */
uip_ds6_addr_t *
uip_ds6_get_link_local(int8_t state)
{
  for(locaddr = uip_ds6_if.addr_list;
      locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
    if(locaddr->isused && (state == -1 || locaddr->state == state)
       && (uip_is_addr_link_local(&locaddr->ipaddr))) {
      if((!locaddr->isinfinite) && (stimestamp_expired(&locaddr->vlifetime))) {
        uip_ds6_addr_rm(locaddr);
      } else {
        return locaddr;
      }
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
/*
 * get a global address -
 * state = -1 => any address is ok. Otherwise state = desired state of addr.
 * (TENTATIVE, PREFERRED, DEPRECATED)
 */
uip_ds6_addr_t *
uip_ds6_get_global(int8_t state)
{
  for(locaddr = uip_ds6_if.addr_list;
      locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
    if(locaddr->isused && (state == -1 || locaddr->state == state)
       && !(uip_is_addr_link_local(&locaddr->ipaddr))) {
      if((!locaddr->isinfinite) && (stimestamp_expired(&locaddr->vlifetime))) {
        uip_ds6_addr_rm(locaddr);
      } else {
        return locaddr;
      }
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uip_ds6_maddr_t *
uip_ds6_maddr_add(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_if.maddr_list, UIP_DS6_MADDR_NB,
      sizeof(uip_ds6_maddr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locmaddr) == FREESPACE) {
    locmaddr->isused = 1;
    uip_ipaddr_copy(&locmaddr->ipaddr, ipaddr);
    return locmaddr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_maddr_rm(uip_ds6_maddr_t * maddr)
{
  if(maddr != NULL) {
    maddr->isused = 0;
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_maddr_t *
uip_ds6_maddr_solicited_node_verify(uip_ds6_maddr_t * maddr)
{
  for(locaddr = uip_ds6_if.addr_list;
      locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
    if(locaddr->isused) {
      if((!locaddr->isinfinite) && (stimestamp_expired(&locaddr->vlifetime))) {
        uip_ds6_addr_rm(locaddr);
      } else if ((locaddr->ipaddr.u8[13] == maddr->ipaddr.u8[13])
          && (locaddr->ipaddr.u16[7] == maddr->ipaddr.u16[7])){
        return maddr;
      }
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uip_ds6_maddr_t *
uip_ds6_maddr_lookup(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_if.maddr_list, UIP_DS6_MADDR_NB,
      sizeof(uip_ds6_maddr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locmaddr) == FOUND) {
    if (uip_is_addr_solicited_node(&locmaddr->ipaddr)) {
      return uip_ds6_maddr_solicited_node_verify(locmaddr);
    } else {
      return locmaddr;
    }
  }
  return NULL;
}


/*---------------------------------------------------------------------------*/
uip_ds6_aaddr_t *
uip_ds6_aaddr_add(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_if.aaddr_list, UIP_DS6_AADDR_NB,
      sizeof(uip_ds6_aaddr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locaaddr) == FREESPACE) {
    locaaddr->isused = 1;
    uip_ipaddr_copy(&locaaddr->ipaddr, ipaddr);
    return locaaddr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_aaddr_rm(uip_ds6_aaddr_t * aaddr)
{
  if(aaddr != NULL) {
    aaddr->isused = 0;
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_aaddr_t *
uip_ds6_aaddr_lookup(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop((uip_ds6_element_t *)uip_ds6_if.aaddr_list,
		       UIP_DS6_AADDR_NB, sizeof(uip_ds6_aaddr_t), ipaddr, 128,
		       (uip_ds6_element_t **)&locaaddr) == FOUND) {
    return locaaddr;
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
int rpl_route_clean(rpl_route_entry_t *state)
{
  return stimer_expired(&state->lifetime);
}
/*---------------------------------------------------------------------------*/
uip_ds6_route_t *
uip_ds6_route_lookup(uip_ipaddr_t *destipaddr)
{
  uip_ds6_route_t *locrt = NULL;
  uint8_t longestmatch = 0;

  PRINTF("DS6: Looking up route for ");
  PRINT6ADDR(destipaddr);
  PRINTF("\n");

  for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB; locroute++) {
    if((locroute->isused) && (locroute->length >= longestmatch)
       &&
       (uip_ipaddr_prefixcmp
        (destipaddr, &locroute->ipaddr, locroute->length))) {
      longestmatch = locroute->length;
      locrt = locroute;
    }
  }

  if(locrt != NULL) {
#ifdef UIP_DS6_ROUTE_STATE_CLEAN
    switch (locrt->state.learned_from) {
      case RPL_ROUTE_FROM_UNICAST_DAO:
      case RPL_ROUTE_FROM_MULTICAST_DAO:
      case RPL_ROUTE_FROM_DIO:
        if(UIP_DS6_ROUTE_STATE_CLEAN(&locrt->state)) {
          uip_ds6_route_rm(locrt);
          //TODO : verify that the prefix len is 128
          mob_lost_node(locrt);
          PRINTF("DS6: Route timeout\n");
          return NULL;
        }
        break;
      default:
        break;
    }
#endif /* UIP_DS6_ROUTE_STATE_TYPE */
    PRINTF("DS6: Found route:");
    PRINT6ADDR(destipaddr);
    PRINTF(" via ");
    PRINT6ADDR(&locrt->nexthop);
    PRINTF("\n");
  } else {
    PRINTF("DS6: No route found\n");
  }

  return locrt;
}

/*---------------------------------------------------------------------------*/
uip_ds6_route_t *
uip_ds6_route_add(uip_ipaddr_t *ipaddr, uint8_t length, uip_ipaddr_t *nexthop,
                  uint8_t metric)
{
  loc_loop_state = uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_routing_table, UIP_DS6_ROUTE_NB,
      sizeof(uip_ds6_route_t), ipaddr, length,
      (uip_ds6_element_t **)&locroute);

#ifdef UIP_DS6_ROUTE_STATE_CLEAN
  if (loc_loop_state == NOSPACE) {
    for(locroute = uip_ds6_routing_table;
        locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
        ++locroute) {
      if(locroute->isused
          && (UIP_DS6_ROUTE_STATE_CLEAN(&locroute->state))) {
        uip_ds6_route_rm(locroute);
        loc_loop_state = FREESPACE;
        break;
      }
    }
  }
  if (loc_loop_state == NOSPACE) {
    locroute = NULL;
  }
#endif /* UIP_DS6_ROUTE_STATE_TYPE */

  if (loc_loop_state == FREESPACE) {
    locroute->isused = 1;
    uip_ipaddr_copy(&(locroute->ipaddr), ipaddr);
    locroute->length = length;
    uip_ipaddr_copy(&(locroute->nexthop), nexthop);
    locroute->metric = metric;

#ifdef UIP_DS6_ROUTE_STATE_TYPE
    memset (&(locroute->state),0,sizeof(UIP_DS6_ROUTE_STATE_TYPE));
#endif

    PRINTF("DS6: adding route: ");
    PRINT6ADDR(ipaddr);
    PRINTF(" via ");
    PRINT6ADDR(nexthop);
    PRINTF("\n");
    ANNOTATE("#L %u 1;blue\n", nexthop->u8[sizeof(uip_ipaddr_t) - 1]);
  }

  return locroute;
}
/*---------------------------------------------------------------------------*/
void
uip_ds6_route_rm(uip_ds6_route_t *route)
{
  route->isused = 0;
}
/*---------------------------------------------------------------------------*/
void
uip_ds6_route_rm_by_nexthop(uip_ipaddr_t *nexthop)
{
  for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB;
      locroute++) {
    if(locroute->isused && uip_ipaddr_cmp(&locroute->nexthop, nexthop)) {
      locroute->isused = 0;
    }
  }
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_select_src(uip_ipaddr_t *src, uip_ipaddr_t *dst)
{
  uint8_t best = 0;             /* number of bit in common with best match */
  uint8_t n = 0;
  uip_ds6_addr_t *matchaddr = NULL;

  if(!uip_is_addr_link_local(dst) && !uip_is_addr_mcast(dst)) {
    /* find longest match */
    for(locaddr = uip_ds6_if.addr_list;
        locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
      /* Only preferred global (not link-local) addresses */
PRINTF("PReff %i ",locaddr->isused && locaddr->state);
PRINT6ADDR(&locaddr->ipaddr);
PRINTF("\n");
      if(locaddr->isused && locaddr->state == ADDR_PREFERRED &&
         !uip_is_addr_link_local(&locaddr->ipaddr)) {
        if((!locaddr->isinfinite) && (stimestamp_expired(&locaddr->vlifetime))) {
          uip_ds6_addr_rm(locaddr);
        } else {
          n = get_match_length(dst, &locaddr->ipaddr);
          if(n >= best) {
            best = n;
            matchaddr = locaddr;
          }
        }
      }
    }
  } else {
    matchaddr = uip_ds6_get_link_local(ADDR_PREFERRED);
  }

  /* use the :: (unspecified address) as source if no match found */
  if(matchaddr == NULL) {
    uip_create_unspecified(src);
  } else {
    uip_ipaddr_copy(src, &matchaddr->ipaddr);
  }
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_set_addr_iid(uip_ipaddr_t *ipaddr, uip_lladdr_t * lladdr)
{
  memcpy(ipaddr->u8 + 8, lladdr, UIP_LLADDR_LEN);
}

/*---------------------------------------------------------------------------*/
uint8_t
get_match_length(uip_ipaddr_t *src, uip_ipaddr_t *dst)
{
  uint8_t j, k, x_or;
  uint8_t len = 0;

  for(j = 0; j < 16; j++) {
    if(src->u8[j] == dst->u8[j]) {
      len += 8;
    } else {
      x_or = src->u8[j] ^ dst->u8[j];
      for(k = 0; k < 8; k++) {
        if((x_or & 0x80) == 0) {
          len++;
          x_or <<= 1;
        } else {
          break;
        }
      }
      break;
    }
  }
  return len;
}


/** @} */
