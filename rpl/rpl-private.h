/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 *
 * Author: Joakim Eriksson, Nicolas Tsiftes
 */

#ifndef RPL_PRIVATE_H
#define RPL_PRIVATE_H

/*
 * ContikiRPL - an implementation of the routing protocol for low power and
 * lossy networks. See: draft-ietf-roll-rpl-17.
 *
 * --
 * The DIOs handle prefix information option for setting global IP addresses
 * on the nodes, but the current handling is not awaiting the join of the DAG
 * so it does not currently support multiple DAGs.
 */

#include "rpl/rpl.h"

#include "lib/list.h"
#include "uip6.h"
#include "sys/clock.h"
#include "sys/event.h"
#include "uip-ds6.h"

/*---------------------------------------------------------------------------*/
/** \brief Is IPv6 address a the link local all rpl nodes multicast address */
#define uip_is_addr_linklocal_rplnodes_mcast(a)     \
  ((((a)->u8[0]) == 0xff) &&                        \
   (((a)->u8[1]) == 0x02) &&                        \
   (((a)->u16[1]) == 0) &&                          \
   (((a)->u16[2]) == 0) &&                          \
   (((a)->u16[3]) == 0) &&                          \
   (((a)->u16[4]) == 0) &&                          \
   (((a)->u16[5]) == 0) &&                          \
   (((a)->u16[6]) == 0) &&                          \
   (((a)->u8[14]) == 0) &&                          \
   (((a)->u8[15]) == 0x1a))

/** \brief set IP address a to the link local all-rpl nodes multicast address */
#define uip_create_linklocal_rplnodes_mcast(a) uip_ip6addr(a, 0xff02, 0, 0, 0, 0, 0, 0, 0x001a)
/*---------------------------------------------------------------------------*/
/* RPL message types */
#define RPL_CODE_DIS                   0x00   /* DAG Information Solicitation */
#define RPL_CODE_DIO                   0x01   /* DAG Information Option */
#define RPL_CODE_DAO                   0x02   /* Destination Advertisement Option */
#define RPL_CODE_DAO_ACK               0x03   /* DAO acknowledgment */
#define RPL_CODE_SEC_DIS               0x80   /* Secure DIS */
#define RPL_CODE_SEC_DIO               0x81   /* Secure DIO */
#define RPL_CODE_SEC_DAO               0x82   /* Secure DAO */
#define RPL_CODE_SEC_DAO_ACK           0x83   /* Secure DAO ACK */

/* RPL DIO/DAO suboption types */
#define RPL_DIO_SUBOPT_PAD1              	0
#define RPL_DIO_SUBOPT_PADN              	1
#define RPL_DIO_SUBOPT_DAG_METRIC_CONTAINER	2
#define RPL_DIO_SUBOPT_ROUTE_INFO        	3
#define RPL_DIO_SUBOPT_DAG_CONF          	4
#define RPL_DIO_SUBOPT_TARGET            	5
#define RPL_DIO_SUBOPT_TRANSIT           	6
#define RPL_DIO_SUBOPT_SOLICITED_INFO    	7
#define RPL_DIO_SUBOPT_PREFIX_INFO       	8
#define RPL_DIO_SUBOPT_OTHER_DODAG       	9

#define RPL_DAO_K_FLAG                   0x80 /* DAO ACK requested */
#define RPL_DAO_D_FLAG                   0x40 /* DODAG ID Present */
/*---------------------------------------------------------------------------*/
/* Default values for RPL constants and variables. */

/* The default value for the DAO timer. */
#define DEFAULT_DAO_LATENCY             (CLOCK_SECOND * 8)

/* Special value indicating immediate removal. */
#define ZERO_LIFETIME                   0

/* Special value indicating that a DAO should not expire. */
#define INFINITE_LIFETIME               0xffffffff

/* Default route lifetime in seconds. */
#define DEFAULT_ROUTE_LIFETIME          INFINITE_LIFETIME

#define DEFAULT_RPL_LIFETIME_UNIT       0xffff
#define DEFAULT_RPL_DEF_LIFETIME        0xff

#ifndef RPL_CONF_MIN_HOPRANKINC
#define DEFAULT_MIN_HOPRANKINC          256
#else
#define DEFAULT_MIN_HOPRANKINC RPL_CONF_MIN_HOPRANKINC
#endif
#define DEFAULT_MAX_RANKINC             (3 * DEFAULT_MIN_HOPRANKINC)

#define DAG_RANK(fixpt_rank, instance)	((fixpt_rank) / (instance)->min_hoprankinc)

/* Rank of a virtual root node that coordinates DAG root nodes. */
#define BASE_RANK                       0

/* Rank of a root node. */
#define ROOT_RANK(instance)                  (instance)->min_hoprankinc

#define INFINITE_RANK                   0xffff

#define NEIGHBOR_INFO_ETX_DIVISOR       16
#define NEIGHBOR_INFO_ETX2FIX(etx)    ((etx) * NEIGHBOR_INFO_ETX_DIVISOR)
#define NEIGHBOR_INFO_FIX2ETX(fix)    ((fix) / NEIGHBOR_INFO_ETX_DIVISOR)
#define INITIAL_LINK_METRIC             NEIGHBOR_INFO_ETX2FIX(3)

/* Represents 2^n ms. */
/* Default value according to the specification is 3 which
   means 8 milliseconds, but that is an unreasonable value if
   using power-saving / duty-cycling    */
#ifdef RPL_CONF_DIO_INTERVAL_MIN
#define DEFAULT_DIO_INTERVAL_MIN        RPL_CONF_DIO_INTERVAL_MIN
#else
#define DEFAULT_DIO_INTERVAL_MIN        12
#endif

/* Maximum amount of timer doublings. */
#ifdef RPL_CONF_DIO_INTERVAL_DOUBLINGS
#define DEFAULT_DIO_INTERVAL_DOUBLINGS  RPL_CONF_DIO_INTERVAL_DOUBLINGS
#else
#define DEFAULT_DIO_INTERVAL_DOUBLINGS  8
#endif

/* Default DIO redundancy. */
#ifdef RPL_CONF_DIO_REDUNDANCY
#define DEFAULT_DIO_REDUNDANCY          RPL_CONF_DIO_REDUNDANCY
#else
#define DEFAULT_DIO_REDUNDANCY          10
#endif

/* Expire DAOs from neighbors that do not respond in this time. (seconds) */
#define DAO_EXPIRATION_TIMEOUT          60
/*---------------------------------------------------------------------------*/
#define RPL_INSTANCE_LOCAL_FLAG         0x80
#define RPL_INSTANCE_D_FLAG             0x40

/* DAG Mode of Operation */
#define RPL_MOP_NO_DOWNWARD_ROUTES      0
#define RPL_MOP_NON_STORING             1
#define RPL_MOP_STORING_NO_MULTICAST    2
#define RPL_MOP_STORING_MULTICAST       3

#ifdef  RPL_CONF_MOP
#define RPL_MOP_DEFAULT                 RPL_CONF_MOP
#else
#define RPL_MOP_DEFAULT                 RPL_MOP_STORING_NO_MULTICAST
#endif

/*
 * The ETX in the metric container is expressed as a fixed-point value 
 * whose integer part can be obtained by dividing the value by 
 * RPL_DAG_MC_ETX_DIVISOR.
 */
#define RPL_DAG_MC_ETX_DIVISOR		128

/* DIS related */
#define RPL_DIS_SEND                    1
#ifdef  RPL_DIS_INTERVAL_CONF
#define RPL_DIS_INTERVAL                RPL_DIS_INTERVAL_CONF
#else
#define RPL_DIS_INTERVAL                60
#endif
#define RPL_DIS_START_DELAY             5
#define RPL_MAX_PERIODIC                254
/*---------------------------------------------------------------------------*/
/* Logical representation of a DAG Information Object (DIO.) */
struct rpl_dio {
  uip_ipaddr_t dag_id;
  rpl_ocp_t ocp;
  rpl_rank_t rank;
  uint8_t grounded;
  uint8_t mop;
  uint8_t preference;
  uint8_t version;
  uint8_t instance_id;
  uint8_t dtsn;
  uint8_t dag_intdoubl;
  uint8_t dag_intmin;
  uint8_t dag_redund;
  uint8_t default_lifetime;
  uint16_t lifetime_unit;
  rpl_rank_t dag_max_rankinc;
  rpl_rank_t dag_min_hoprankinc;
  rpl_prefix_t destination_prefix;
  rpl_prefix_t prefix_info;
  struct rpl_metric_container mc;
};
typedef struct rpl_dio rpl_dio_t;

#if RPL_CONF_STATS
/* Statistics for fault management. */
struct rpl_stats {
  uint16_t mem_overflows;
  uint16_t local_repairs;
  uint16_t global_repairs;
  uint16_t malformed_msgs;
  uint16_t resets;
  uint16_t parent_switch;
};
typedef struct rpl_stats rpl_stats_t;

extern rpl_stats_t rpl_stats;
#endif
/*---------------------------------------------------------------------------*/
/* RPL macros. */

#if RPL_CONF_STATS
#define RPL_STAT(code)	(code) 
#else
#define RPL_STAT(code)
#endif /* RPL_CONF_STATS */
/*---------------------------------------------------------------------------*/
/* Instances */
extern rpl_instance_t instance_table[];
rpl_instance_t *default_instance;

/* ICMPv6 functions for RPL. */
void dis_output(uip_ipaddr_t *addr);
void dio_output(rpl_instance_t *, uip_ipaddr_t *uc_addr);
void dao_ack_output(rpl_instance_t *, uip_ipaddr_t *, uint8_t);
void uip_rpl_input(void);

/* RPL Header Option */
int rpl_verify_header(int uip_ext_opt_offset);
void rpl_update_header_empty();
int rpl_update_header_final(uip_ipaddr_t *addr);

/* RPL logic functions. */
void rpl_join_dag(uip_ipaddr_t *from, rpl_dio_t *dio);
void rpl_join_instance(uip_ipaddr_t *from, rpl_dio_t *dio);
void rpl_local_repair(rpl_instance_t *instance);
void rpl_process_dio(uip_ipaddr_t *, rpl_dio_t *);

/* DAG object management. */
rpl_dag_t *rpl_alloc_dodag(uint8_t, uip_ipaddr_t *);
rpl_instance_t *rpl_alloc_instance(uint8_t);
void rpl_free_dodag(rpl_dag_t *);
void rpl_free_instance(rpl_instance_t *);

/* RPL routing table functions. */
void rpl_remove_routes(rpl_dag_t *dag);
void rpl_remove_routes_by_nexthop(uip_ipaddr_t *nexthop, rpl_dag_t *dag);
uip_ds6_route_t *rpl_add_route(rpl_dag_t *dag, uip_ipaddr_t *prefix,
                               int prefix_len, uip_ipaddr_t *next_hop,
                               uint8_t learned_from);

/* Objective function. */
rpl_of_t *rpl_find_of(rpl_ocp_t);

/* Timer functions. */
void rpl_reset_dio_timer(rpl_instance_t *, uint8_t);
ev_tstamp dio_rescheduler(struct ev_periodic *w, ev_tstamp now);

#endif /* RPL_PRIVATE_H */
