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
#include "uip-nd6.h"
#include "uip-ds6.h"
#include "sys/event.h"

#define DEBUG DEBUG_NONE
#include "uip-debug.h"

struct ev_timer uip_ds6_timer_periodic;                                /** \brief Timer for maintenance of data structures */
struct stimestamp next_timer;

#if UIP_ND6_SEND_RA
static uint8_t racount;                                         /** \brief number of RA already sent */
static uint16_t rand_time;                                      /** \brief random time value for timers */
struct stimer uip_ds6_timer_ra;                                 /** \brief RA timer, to schedule RA sending */
#endif

/** \name "DS6" Data structures */
/** @{ */
uip_ds6_netif_t uip_ds6_if;                                       /** \brief The single interface */
uip_ds6_nbr_t uip_ds6_nbr_cache[UIP_DS6_NBR_NB];                  /** \brief Neighor cache */
uip_ds6_defrt_t uip_ds6_defrt_list[UIP_DS6_DEFRT_NB];             /** \brief Default rt list */
uip_ds6_prefix_t uip_ds6_prefix_list[UIP_DS6_PREFIX_NB];          /** \brief Prefix list */
uip_ds6_route_t uip_ds6_routing_table[UIP_DS6_ROUTE_NB];          /** \brief Routing table */

/** @} */

/* "full" (as opposed to pointer) ip address used in this file,  */
static uip_ipaddr_t loc_fipaddr;

/* Pointers used in this file */
static uip_ipaddr_t *locipaddr;
static uip_ds6_addr_t *locaddr;
static uip_ds6_maddr_t *locmaddr;
static uip_ds6_aaddr_t *locaaddr;
static uip_ds6_prefix_t *locprefix;
static uip_ds6_nbr_t *locnbr;
static uip_ds6_defrt_t *locdefrt;
static uip_ds6_route_t *locroute;

/* uint8_t used in this file */
static uint8_t loc_loop_state;

void rpl_ipv6_neighbor_callback(uip_ds6_nbr_t *nbr);

/*---------------------------------------------------------------------------*/
void
uip_ds6_init(void)
{
  PRINTF("Init of IPv6 data structures\n");
  PRINTF("%u neighbors\n%u default routers\n%u prefixes\n%u routes\n%u unicast addresses\n%u multicast addresses\n%u anycast addresses\n",
     UIP_DS6_NBR_NB, UIP_DS6_DEFRT_NB, UIP_DS6_PREFIX_NB, UIP_DS6_ROUTE_NB,
     UIP_DS6_ADDR_NB, UIP_DS6_MADDR_NB, UIP_DS6_AADDR_NB);
  memset(uip_ds6_nbr_cache, 0, sizeof(uip_ds6_nbr_cache));
  memset(uip_ds6_defrt_list, 0, sizeof(uip_ds6_defrt_list));
  memset(uip_ds6_prefix_list, 0, sizeof(uip_ds6_prefix_list));
  memset(&uip_ds6_if, 0, sizeof(uip_ds6_if));
  memset(uip_ds6_routing_table, 0, sizeof(uip_ds6_routing_table));

  /* Set interface parameters */
  uip_ds6_if.link_mtu = UIP_LINK_MTU;
  uip_ds6_if.cur_hop_limit = UIP_TTL;
  uip_ds6_if.base_reachable_time = UIP_ND6_REACHABLE_TIME;
  uip_ds6_if.reachable_time = uip_ds6_compute_reachable_time();
  uip_ds6_if.retrans_timer = UIP_ND6_RETRANS_TIMER;
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
#if UIP_ND6_SEND_RA
  stimer_set(&uip_ds6_timer_ra, 2);     /* wait to have a link local IP address */
#endif /* UIP_ND6_SEND_RA */
  PRINTF("DS: init set timer to %u\n",UIP_DS6_PERIOD);
  stimestamp_set(&next_timer,UIP_DS6_PERIOD);
  ev_timer_init(&uip_ds6_timer_periodic,uip_ds6_periodic,0,UIP_DS6_PERIOD);
  ev_timer_start(event_loop,&uip_ds6_timer_periodic);
  return;
}


/*--------------------------------------------------------------------------*/
void
uip_ds6_periodic(struct ev_loop *loop, struct ev_timer *w, int revents)
{
  clock_time_t next_period=UIP_DS6_MAX_PERIOD;
  clock_time_t temp_next;

  /* Periodic processing on neighbors */
  for(locnbr = uip_ds6_nbr_cache;
      locnbr < uip_ds6_nbr_cache + UIP_DS6_NBR_NB;
      locnbr++) {
    if(locnbr->isused) {
      switch(locnbr->state) {
      case NBR_INCOMPLETE:
        if(locnbr->nscount >= UIP_ND6_MAX_MULTICAST_SOLICIT) {
          if((locdefrt = uip_ds6_defrt_lookup(&locnbr->ipaddr)) != NULL) {
            uip_ds6_defrt_rm(locdefrt);
          }
          uip_ds6_route_rm_by_nexthop(&locnbr->ipaddr);
          uip_ds6_nbr_rm(locnbr);
        } else if(stimestamp_expired(&locnbr->sendns)) {
          locnbr->nscount++;
          PRINTF("NBR_INCOMPLETE: NS %u\n", locnbr->nscount);
          uip_nd6_ns_output(NULL, NULL, &locnbr->ipaddr);
          stimestamp_set(&locnbr->sendns, uip_ds6_if.retrans_timer / 1000);
          if ((next_period==0)
              || ((temp_next=stimestamp_remaining(&locnbr->sendns)) < next_period)) {
            next_period=temp_next;
          }
        } else if ((next_period==0)
           || ((temp_next=stimestamp_remaining(&locnbr->sendns)) < next_period)) {
          next_period=temp_next;
        }
        break;
      case NBR_REACHABLE:
        if(stimestamp_expired(&locnbr->reachable)) {
          PRINTF("REACHABLE: moving to STALE (");
          PRINT6ADDR(&locnbr->ipaddr);
          PRINTF(")\n");
          locnbr->state = NBR_STALE;
        }
        break;
      case NBR_DELAY:
        if(stimestamp_expired(&locnbr->reachable)) {
          locnbr->state = NBR_PROBE;
          locnbr->nscount = 1;
          PRINTF("DELAY: moving to PROBE + NS %u\n", locnbr->nscount);
          uip_nd6_ns_output(NULL, &locnbr->ipaddr, &locnbr->ipaddr);
          stimestamp_set(&locnbr->sendns, uip_ds6_if.retrans_timer / 1000);
          if ((next_period==0)
              || ((temp_next=stimestamp_remaining(&locnbr->sendns)) < next_period)) {
            next_period=temp_next;
          }
        } else if ((next_period==0)
           || ((temp_next=stimestamp_remaining(&locnbr->reachable)) < next_period)) {
          next_period=temp_next;
        }
        break;
      case NBR_PROBE:
        if(locnbr->nscount >= UIP_ND6_MAX_UNICAST_SOLICIT) {
          PRINTF("PROBE END (FAILED)\n");
          if((locdefrt = uip_ds6_defrt_lookup(&locnbr->ipaddr)) != NULL) {
            uip_ds6_defrt_rm(locdefrt);
          }
          uip_ds6_route_rm_by_nexthop(&locnbr->ipaddr);
          uip_ds6_nbr_rm(locnbr);
        } else if(stimestamp_expired(&locnbr->sendns)) {
          locnbr->nscount++;
          PRINTF("PROBE: NS %u\n", locnbr->nscount);
          uip_nd6_ns_output(NULL, &locnbr->ipaddr, &locnbr->ipaddr);
          stimestamp_set(&locnbr->sendns, uip_ds6_if.retrans_timer / 1000);
          if ((next_period==0)
              || ((temp_next=stimestamp_remaining(&locnbr->sendns)) < next_period)) {
            next_period=temp_next;
          }
        } else if ((next_period==0)
           || ((temp_next=stimestamp_remaining(&locnbr->sendns)) < next_period)) {
          next_period=temp_next;

        }
        break;
      default:
        break;
      }
    }
  }

#if UIP_ND6_SEND_RA
  /* Periodic RA sending */
  if(stimer_expired(&uip_ds6_timer_ra)) {
    uip_ds6_send_ra_periodic();
  } else if((next_period==0)
     || ((temp_next=stimer_remaining(&uip_ds6_timer_ra)) < next_period)) {
   next_period=temp_next;
  }
#endif /* UIP_ND6_SEND_RA */
  PRINTF("DS: periodic set timer to %lu\n",next_period);
  stimestamp_set(&next_timer,next_period);
  w->repeat=next_period;
  ev_timer_again(event_loop,w);
  return;
}

/*---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------*/
uint8_t
uip_ds6_list_clean(uip_ds6_timed_element_t *list, uint8_t size,
                  uint16_t elementsize, uint8_t type,
                  uip_ds6_timed_element_t **out_element)
{
  uip_ds6_timed_element_t *element;

  for(element = list;
      element <
      (uip_ds6_timed_element_t *)((uint8_t *)list + (size * elementsize));
      element = (uip_ds6_timed_element_t *)((uint8_t *)element + elementsize)) {
    if(element->isused) {
      if(!element->isinfinite
            && stimestamp_expired(&(element->lifetime))) {
        switch(type) {
          case UIP_DS6_ADDR:
            uip_ds6_addr_rm((uip_ds6_addr_t *)element);
            break;
          case UIP_DS6_DEFRT:
            uip_ds6_defrt_rm((uip_ds6_defrt_t *)element);
            break;
          case UIP_DS6_PREFIX:
            uip_ds6_prefix_rm((uip_ds6_prefix_t *)element);
            break;
          default:
            element->isused=0;
        }
        *out_element = element;
        return FREESPACE;
      }
    } else {
      *out_element = element;
      return FREESPACE;
    }
  }
  *out_element = NULL;
  return NOSPACE;
}

/*---------------------------------------------------------------------------*/
uip_ds6_nbr_t *
uip_ds6_nbr_add(uip_ipaddr_t *ipaddr, uip_lladdr_t * lladdr,
                uint8_t isrouter, uint8_t state)
{
  loc_loop_state = uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_nbr_cache, UIP_DS6_NBR_NB,
      sizeof(uip_ds6_nbr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locnbr);

  if(loc_loop_state == NOSPACE) {
    uip_ds6_nbr_t *n;
    clock_time_t oldest_time;

    locnbr = NULL;
    oldest_time = clock_time();

    for(n = uip_ds6_nbr_cache;
        n < &uip_ds6_nbr_cache[UIP_DS6_NBR_NB];
        n++) {
      if(n->isused) {
        if(n->last_lookup < oldest_time) {
          locnbr = n;
          oldest_time = n->last_lookup;
        }
      }
    }
    if(locnbr != NULL) {
      uip_ds6_nbr_rm(locnbr);
      loc_loop_state = FREESPACE;
    }
  }

  if(loc_loop_state == FREESPACE) {
    locnbr->isused = 1;
    uip_ipaddr_copy(&locnbr->ipaddr, ipaddr);
    if(lladdr != NULL) {
      memcpy(&locnbr->lladdr, lladdr, UIP_LLADDR_LEN);
    } else {
      memset(&locnbr->lladdr, 0, UIP_LLADDR_LEN);
    }
    locnbr->isrouter = isrouter;
    locnbr->state = state;
    /* timestamps are set separately, for now we put them in expired state */
    stimestamp_set(&locnbr->reachable, 0);
    stimestamp_set(&locnbr->sendns, 0);
    locnbr->nscount = 0;
    PRINTF("Adding neighbor with ip addr ");
    PRINT6ADDR(ipaddr);
    PRINTF("link addr ");
    PRINTLLADDR((&(locnbr->lladdr)));
    PRINTF("state %u\n", state);
    rpl_ipv6_neighbor_callback(locnbr);
    locnbr->last_lookup = clock_time();
    if ((state == NBR_INCOMPLETE)
        && (stimestamp_remaining(&next_timer) > UIP_DS6_PERIOD)) {
      PRINTF("DS: addd set timer to %u\n",UIP_DS6_PERIOD);
      stimestamp_set(&next_timer,UIP_DS6_PERIOD);
      uip_ds6_timer_periodic.repeat=UIP_DS6_PERIOD;
      ev_timer_again(event_loop,&uip_ds6_timer_periodic);
    }
    return locnbr;
  }
  PRINTF("uip_ds6_nbr_add drop\n");
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_nbr_rm(uip_ds6_nbr_t *nbr)
{
  if(nbr != NULL) {
    nbr->isused = 0;
    rpl_ipv6_neighbor_callback(nbr);
  }
  return;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_nbr_nud(uip_ds6_nbr_t *nbr)
{
  clock_time_t temp_next;

  if(nbr->state == NBR_STALE) {
    nbr->state = NBR_DELAY;
    stimestamp_set(&(nbr->reachable), UIP_ND6_DELAY_FIRST_PROBE_TIME);
    nbr->nscount = 0;
    PRINTF("tcpip_ipv6_output: nbr cache entry stale moving to delay\n");
    temp_next=stimestamp_remaining(&nbr->reachable);
    if (stimestamp_remaining(&next_timer) > temp_next) {
      PRINTF("DS: addd set timer to %u\n", temp_next);
      stimestamp_set(&next_timer, temp_next);
      uip_ds6_timer_periodic.repeat = temp_next;
      ev_timer_again(event_loop,&uip_ds6_timer_periodic);
    }
  }
}
/*---------------------------------------------------------------------------*/
uip_ds6_nbr_t *
uip_ds6_nbr_lookup(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_nbr_cache, UIP_DS6_NBR_NB,
      sizeof(uip_ds6_nbr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locnbr) == FOUND) {
    if(stimestamp_expired(&locnbr->reachable)) {
      PRINTF("REACHABLE: moving to STALE (");
      PRINT6ADDR(&locnbr->ipaddr);
      PRINTF(")\n");
      locnbr->state = NBR_STALE;
    }
    return locnbr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uip_ds6_nbr_t *
uip_ds6_nbr_ll_lookup(uip_lladdr_t *lladdr)
{
  uip_ds6_nbr_t *fin;

  for( locnbr=uip_ds6_nbr_cache, fin=locnbr + UIP_DS6_NBR_NB;
       locnbr<fin;
       ++locnbr) {
    if(locnbr->isused) {
      if(!memcmp(lladdr,&locnbr->lladdr,UIP_LLADDR_LEN)) {
        if(stimestamp_expired(&locnbr->reachable)) {
          PRINTF("REACHABLE: moving to STALE (");
          PRINT6ADDR(&locnbr->ipaddr);
          PRINTF(")\n");
          locnbr->state = NBR_STALE;
        }
        return locnbr;
      }
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uip_ds6_defrt_t *
uip_ds6_defrt_add(uip_ipaddr_t *ipaddr, unsigned long interval)
{
  loc_loop_state = uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_defrt_list, UIP_DS6_DEFRT_NB,
      sizeof(uip_ds6_defrt_t), ipaddr, 128,
      (uip_ds6_element_t **)&locdefrt);

  if( loc_loop_state == NOSPACE ) {
    loc_loop_state = uip_ds6_list_clean
     ((uip_ds6_timed_element_t *)uip_ds6_defrt_list, UIP_DS6_DEFRT_NB,
      sizeof(uip_ds6_addr_t), UIP_DS6_DEFRT,
      (uip_ds6_timed_element_t **)&locaddr);
  }

  if( loc_loop_state == FREESPACE) {
    locdefrt->isused = 1;
    uip_ipaddr_copy(&locdefrt->ipaddr, ipaddr);
    if(interval != 0) {
      stimestamp_set(&locdefrt->lifetime, interval);
      locdefrt->isinfinite = 0;
    } else {
      locdefrt->isinfinite = 1;
    }

    PRINTF("Adding defrouter with ip addr ");
    PRINT6ADDR(&locdefrt->ipaddr);
    PRINTF("\n");

    ANNOTATE("#L %u 1\n", ipaddr->u8[sizeof(uip_ipaddr_t) - 1]);

    return locdefrt;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_defrt_rm(uip_ds6_defrt_t *defrt)
{
  if(defrt != NULL) {
    defrt->isused = 0;
    ANNOTATE("#L %u 0\n", defrt->ipaddr.u8[sizeof(uip_ipaddr_t) - 1]);
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_defrt_t *
uip_ds6_defrt_lookup(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop((uip_ds6_element_t *)uip_ds6_defrt_list,
		       UIP_DS6_DEFRT_NB, sizeof(uip_ds6_defrt_t), ipaddr, 128,
		       (uip_ds6_element_t **)&locdefrt) == FOUND) {
    if((locdefrt->isused) && (!locdefrt->isinfinite) &&
       (stimestamp_expired(&(locdefrt->lifetime)))) {
      uip_ds6_defrt_rm(locdefrt);
      return NULL;
    } else {
      return locdefrt;
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uip_ipaddr_t *
uip_ds6_defrt_choose(void)
{
  uip_ds6_nbr_t *bestnbr;

  locipaddr = NULL;
  for(locdefrt = uip_ds6_defrt_list;
      locdefrt < uip_ds6_defrt_list + UIP_DS6_DEFRT_NB; locdefrt++) {
    if(locdefrt->isused) {
      if((!locdefrt->isinfinite) &&
          (stimestamp_expired(&(locdefrt->lifetime)))) {
        uip_ds6_defrt_rm(locdefrt);
      } else {
        PRINTF("Defrt, IP address ");
        PRINT6ADDR(&locdefrt->ipaddr);
        PRINTF("\n");
        bestnbr = uip_ds6_nbr_lookup(&locdefrt->ipaddr);
        if(bestnbr != NULL && bestnbr->state != NBR_INCOMPLETE) {
          PRINTF("Defrt found, IP address ");
          PRINT6ADDR(&locdefrt->ipaddr);
          PRINTF("\n");
          return &locdefrt->ipaddr;
        } else {
          locipaddr = &locdefrt->ipaddr;
          PRINTF("Defrt INCOMPLETE found, IP address ");
          PRINT6ADDR(&locdefrt->ipaddr);
          PRINTF("\n");
        }
      }
    }
  }
  return locipaddr;
}

/*---------------------------------------------------------------------------*/
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
uip_ds6_addr_t *
uip_ds6_addr_add(uip_ipaddr_t *ipaddr, unsigned long vlifetime, uint8_t type)
{
  loc_loop_state = uip_ds6_list_loop
     ((uip_ds6_element_t *)uip_ds6_if.addr_list, UIP_DS6_ADDR_NB,
      sizeof(uip_ds6_addr_t), ipaddr, 128,
      (uip_ds6_element_t **)&locaddr);

  if( loc_loop_state == NOSPACE ) {
    loc_loop_state = uip_ds6_list_clean
     ((uip_ds6_timed_element_t *)uip_ds6_if.addr_list, UIP_DS6_ADDR_NB,
      sizeof(uip_ds6_addr_t), UIP_DS6_ADDR,
      (uip_ds6_timed_element_t **)&locaddr);
  }

  if( loc_loop_state == FREESPACE) {
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
  return stimestamp_expired(&state->lifetime);
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
    if(UIP_DS6_ROUTE_STATE_CLEAN(&locrt->state)) {
      uip_ds6_route_rm(locrt);
      PRINTF("DS6: Route timeout\n");
      return NULL;
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
  ipaddr->u8[8] ^= 0x02;
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

/*---------------------------------------------------------------------------*/
#if UIP_ND6_SEND_RA
void
uip_ds6_send_ra_sollicited(void)
{
  /* We have a pb here: RA timer max possible value is 1800s,
   * hence we have to use stimers. However, when receiving a RS, we
   * should delay the reply by a random value between 0 and 500ms timers.
   * stimers are in seconds, hence we cannot do this. Therefore we just send
   * the RA (setting the timer to 0 below). We keep the code logic for
   * the days contiki will support appropriate timers */
  rand_time = 0;
  PRINTF("Solicited RA, random time %u\n", rand_time);

  if(stimer_remaining(&uip_ds6_timer_ra) > rand_time) {
    if(stimer_elapsed(&uip_ds6_timer_ra) < UIP_ND6_MIN_DELAY_BETWEEN_RAS) {
      /* Ensure that the RAs are rate limited */
/*      stimer_set(&uip_ds6_timer_ra, rand_time +
                 UIP_ND6_MIN_DELAY_BETWEEN_RAS -
                 stimer_elapsed(&uip_ds6_timer_ra));
  */ } else {
      stimer_set(&uip_ds6_timer_ra, rand_time);
    }
  }
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_send_ra_periodic(void)
{
  if(racount > 0) {
    /* send previously scheduled RA */
    uip_nd6_ra_output(NULL);
    PRINTF("Sending periodic RA\n");
  }

  rand_time = UIP_ND6_MIN_RA_INTERVAL + random_rand() %
    (uint16_t) (UIP_ND6_MAX_RA_INTERVAL - UIP_ND6_MIN_RA_INTERVAL);
  PRINTF("Random time 1 = %u\n", rand_time);

  if(racount < UIP_ND6_MAX_INITIAL_RAS) {
    if(rand_time > UIP_ND6_MAX_INITIAL_RA_INTERVAL) {
      rand_time = UIP_ND6_MAX_INITIAL_RA_INTERVAL;
      PRINTF("Random time 2 = %u\n", rand_time);
    }
    racount++;
  }
  PRINTF("Random time 3 = %u\n", rand_time);
  stimer_set(&uip_ds6_timer_ra, rand_time);
}

#endif /* UIP_ND6_SEND_RA */
/*---------------------------------------------------------------------------*/
uint32_t
uip_ds6_compute_reachable_time(void)
{
  return (uint32_t) (UIP_ND6_MIN_RANDOM_FACTOR
                     (uip_ds6_if.base_reachable_time)) +
    ((uint16_t) (random_rand() << 8) +
     (uint16_t) random_rand()) %
    (uint32_t) (UIP_ND6_MAX_RANDOM_FACTOR(uip_ds6_if.base_reachable_time) -
                UIP_ND6_MIN_RANDOM_FACTOR(uip_ds6_if.base_reachable_time));
}


/** @} */
