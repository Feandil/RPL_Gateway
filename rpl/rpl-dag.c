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
 */
/**
 * \file
 *         Logic for Directed Acyclic Graphs in RPL.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */


#include "conf.h"
#include "rpl/rpl-private.h"
#include "uip6.h"
#include "sys/event.h"

#include <limits.h>
#include <string.h>

#define DEBUG 1
#include "uip-debug.h"

/************************************************************************/
extern rpl_of_t RPL_OF;
static rpl_of_t * const objective_functions[] = {&RPL_OF};

/************************************************************************/
#ifndef RPL_CONF_MAX_PARENTS_PER_DODAG
#define RPL_MAX_PARENTS_PER_DODAG       8
#else
#define RPL_MAX_PARENTS_PER_DODAG       RPL_CONF_MAX_PARENTS_PER_DODAG
#endif /* !RPL_CONF_MAX_PARENTS */

/************************************************************************/
/* RPL definitions. */

#ifndef RPL_CONF_GROUNDED
#define RPL_GROUNDED                    0
#else
#define RPL_GROUNDED                    RPL_CONF_GROUNDED
#endif /* !RPL_CONF_GROUNDED */

#ifndef RPL_CONF_DIO_INTERVAL_MIN
#define RPL_DIO_INTERVAL_MIN            DEFAULT_DIO_INTERVAL_MIN
#else
#define RPL_DIO_INTERVAL_MIN            RPL_CONF_DIO_INTERVAL_MIN
#endif /* !RPL_CONF_DIO_INTERVAL_MIN */

#ifndef RPL_CONF_DIO_INTERVAL_DOUBLINGS
#define RPL_DIO_INTERVAL_DOUBLINGS      DEFAULT_DIO_INTERVAL_DOUBLINGS
#else
#define RPL_DIO_INTERVAL_DOUBLINGS      RPL_CONF_DIO_INTERVAL_DOUBLINGS
#endif /* !RPL_CONF_DIO_INTERVAL_DOUBLINGS */

/************************************************************************/
/* Allocate instance table. */
rpl_instance_t instance_table[RPL_MAX_INSTANCES];

/************************************************************************/
rpl_dag_t *
rpl_set_root(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_dag_t *dag;
  rpl_instance_t *instance;
  uint8_t version;

  version = RPL_LOLLIPOP_INIT;
  dag = rpl_get_dodag(instance_id, dag_id);
  if(dag != NULL) {
    version = dag->version;
    RPL_LOLLIPOP_INCREMENT(version);
    PRINTF("RPL: Dropping a joined DAG when setting this node as root");
    if(dag==dag->instance->current_dag) {
      dag->instance->current_dag=NULL;
    }
    rpl_free_dodag(dag);
  }

  dag = rpl_alloc_dodag(instance_id,dag_id);
  if(dag == NULL) {
    PRINTF("RPL: Failed to allocate a DAG\n");
    return NULL;
  }

  instance = dag->instance;

  dag->version = version;
  dag->joined = 1;
  dag->grounded = RPL_GROUNDED;
  instance->mop = RPL_MOP_DEFAULT;
  instance->of = &RPL_OF;

  memcpy(&dag->dag_id, dag_id, sizeof(dag->dag_id));

  instance->dio_intdoubl = DEFAULT_DIO_INTERVAL_DOUBLINGS;
  instance->dio_intmin = DEFAULT_DIO_INTERVAL_MIN;
  instance->dio_redundancy = DEFAULT_DIO_REDUNDANCY;
  instance->max_rankinc = DEFAULT_MAX_RANKINC;
  instance->min_hoprankinc = DEFAULT_MIN_HOPRANKINC;
  instance->default_lifetime = DEFAULT_RPL_DEF_LIFETIME;
  instance->lifetime_unit = DEFAULT_RPL_LIFETIME_UNIT;

  dag->rank = ROOT_RANK(instance);

  if(instance->current_dag != dag && instance->current_dag != NULL) {
    /* Remove routes installed by DAOs. */
    rpl_remove_routes(instance->current_dag);

    (instance->current_dag)->joined = 0;
  }

  instance->current_dag = dag;
  instance->dtsn_out=RPL_LOLLIPOP_INIT;
  instance->of->update_metric_container(instance);
  default_instance = instance;

  PRINTF("RPL: Node set to be a DAG root with DAG ID ");
  PRINT6ADDR(&dag->dag_id);
  PRINTF("\n");

  ANNOTATE("#A root=%u\n",dag->dag_id.u8[sizeof(dag->dag_id) - 1]);

  instance->dio_timer.instance=instance;

  rpl_reset_dio_timer(instance, 1);

  return dag;
}
/************************************************************************/
int
rpl_repair_root(uint8_t instance_id)
{
  rpl_instance_t * instance;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL) {
    return 0;
  }

  if(instance->current_dag->rank == ROOT_RANK(instance)) {
    RPL_LOLLIPOP_INCREMENT(instance->current_dag->version);
    RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
    rpl_reset_dio_timer(instance, 1);
    return 1;
  }
  return 0;
}
/************************************************************************/
int
rpl_set_prefix(rpl_dag_t *dag, uip_ipaddr_t *prefix, int len)
{
  uip_ipaddr_t ipaddr;

  if(len <= 128) {
    memset(&dag->prefix_info.prefix, 0, 16);
    memcpy(&dag->prefix_info.prefix, prefix, (len + 7) / 8);
    dag->prefix_info.length = len;
    dag->prefix_info.flags = 0x40;
    PRINTF("RPL: Prefix set - will announce this in DIOs\n");

    /* Autoconfigure an address if this node does not already have an address
       with this prefix. */
    /* assume that the prefix ends with zeros! */
    memcpy(&ipaddr, &dag->prefix_info.prefix, 16);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    if(uip_ds6_addr_lookup(&ipaddr) == NULL) {
      PRINTF("RPL: adding global IP address ");
      PRINT6ADDR(&ipaddr);
      PRINTF("\n");
      uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
    }
    return 1;
  }
  return 0;
}
/************************************************************************/
int
rpl_matching_used_prefix(uip_ipaddr_t *prefix)
{
  int i;
  rpl_instance_t *instance, *end;

  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES; instance < end; ++instance) {
    if(instance->used) {
      for(i = 0; i < RPL_MAX_DODAG_PER_INSTANCE; ++i) {
        if((instance->dag_table[i].used)
            && (uip_ipaddr_prefixcmp(&instance->dag_table[i].prefix_info.prefix, prefix, instance->dag_table[i].prefix_info.length))) {
          return 1;
        }
      }
    }
  }
  return 0;
}
/************************************************************************/
rpl_instance_t *
rpl_alloc_instance(uint8_t instance_id)
{
  rpl_instance_t *instance, *end;

  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES; instance < end; ++instance) {
    if(instance->used == 0) {
      memset(instance, 0, sizeof(*instance));
      instance->instance_id = instance_id;
      instance->used = 1;
      return instance;
    }
  }
  return NULL;
}
/************************************************************************/
rpl_dag_t *
rpl_alloc_dodag(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_dag_t *dag, *end;
  rpl_instance_t *instance;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL ) {
    instance = rpl_alloc_instance(instance_id);
    if(instance == NULL ) {
      RPL_STAT(rpl_stats.mem_overflows++);
      return NULL;
    }
    dag = &instance->dag_table[0];
    dag->used = 1;
    dag->rank = INFINITE_RANK;
    dag->instance = instance;
    instance->current_dag = dag;
    return dag;
  }

  for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DODAG_PER_INSTANCE; dag < end; ++dag) {
    if(!dag->used) {
      memset(dag, 0, sizeof(*dag));
      dag->used = 1;
      dag->rank = INFINITE_RANK;
      dag->instance = instance;
      return dag;
    }
  }
  return NULL;
}
/************************************************************************/
void
rpl_set_default_instance(rpl_instance_t *instance)
{
  default_instance = instance;
}
/************************************************************************/
void
rpl_free_instance(rpl_instance_t *instance)
{
  rpl_dag_t *dag;
  rpl_dag_t *end;

  PRINTF("RPL: Leaving the instance %u\n", instance->instance_id);

  /* Remove any DODAG inside this instance */
  for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DODAG_PER_INSTANCE; dag < end; ++dag) {
    if(dag->used) {
      rpl_free_dodag(dag);
    }
  }

  ev_periodic_stop(event_loop,&instance->dio_timer.periodic);

  if(default_instance == instance) {
    default_instance = NULL;
  }

  instance->used = 0;
}
/************************************************************************/
void
rpl_free_dodag(rpl_dag_t *dag)
{
  if(dag->joined) {
    PRINTF("RPL: Leaving the DAG ");
    PRINT6ADDR(&dag->dag_id);
    PRINTF("\n");
    dag->joined = 0;
  }
  dag->used = 0;
}
/************************************************************************/
rpl_dag_t *
rpl_get_any_dag(void)
{
  int i;

  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used && instance_table[i].current_dag->joined) {
      return instance_table[i].current_dag;
    }
  }
  return NULL;
}
/************************************************************************/
rpl_instance_t *
rpl_get_instance(uint8_t instance_id)
{
  int i;

  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used && instance_table[i].instance_id == instance_id) {
      return &instance_table[i];
    }
  }
  return NULL;
}
/************************************************************************/
rpl_dag_t *
rpl_get_dodag(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_instance_t *instance, *end;
  int i;

  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES; instance < end; ++instance) {
    if(instance->used && instance->instance_id == instance_id) {
      for(i = 0; i < RPL_MAX_DODAG_PER_INSTANCE; ++i) {
        if(instance->dag_table[i].used && (!memcmp(&instance->dag_table[i].dag_id, dag_id, sizeof(*dag_id)))) {
          return &instance->dag_table[i];
        }
      }
    }
  }
  return NULL;
}
/************************************************************************/
rpl_of_t *
rpl_find_of(rpl_ocp_t ocp)
{
  unsigned int i;

  for(i = 0;
      i < sizeof(objective_functions) / sizeof(objective_functions[0]);
      i++) {
    if(objective_functions[i]->ocp == ocp) {
      return objective_functions[i];
    }
  }

  return NULL;
}
/************************************************************************/
