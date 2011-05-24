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

#include "rpl/rpl-private.h"

#define DEBUG 1
#include "uip-debug.h"

static void reset(rpl_dag_t *);
static void update_metric_container(rpl_instance_t *);

rpl_of_t rpl_of_etx = {
  reset,
  update_metric_container,
  1
};

#define NI_ETX_TO_RPL_ETX(etx)						\
	((etx) * (RPL_DAG_MC_ETX_DIVISOR / NEIGHBOR_INFO_ETX_DIVISOR))
#define RPL_ETX_TO_NI_ETX(etx)					\
	((etx) / (RPL_DAG_MC_ETX_DIVISOR / NEIGHBOR_INFO_ETX_DIVISOR))

/* Reject parents that have a higher link metric than the following. */
#define MAX_LINK_METRIC			10

/* Reject parents that have a higher path cost than the following. */
#define MAX_PATH_COST			100

/*
 * The rank must differ more than 1/PARENT_SWITCH_THRESHOLD_DIV in order
 * to switch preferred parent.
 */
#define PARENT_SWITCH_THRESHOLD_DIV	2

typedef uint16_t rpl_path_metric_t;

static void
reset(rpl_dag_t *sag)
{
}

static void
update_metric_container(rpl_instance_t *instance)
{
  rpl_path_metric_t path_metric;
  rpl_dag_t *dag;
#if RPL_DAG_MC == RPL_DAG_MC_ENERGY
  uint8_t type;
#endif

  instance->mc.flags = RPL_DAG_MC_FLAG_P;
  instance->mc.aggr = RPL_DAG_MC_AGGR_ADDITIVE;
  instance->mc.prec = 0;

  dag = instance->current_dag;

  if (!dag->joined) {
    /* We should probably do something here */
    return;
  }

  if(dag->rank == ROOT_RANK(instance)) {
    path_metric = 0;
  }

#if RPL_DAG_MC == RPL_DAG_MC_ETX

  instance->mc.type = RPL_DAG_MC_ETX;
  instance->mc.length = sizeof(instance->mc.obj.etx);
  instance->mc.obj.etx = path_metric;

  PRINTF("RPL: My path ETX to the root is %u.%u\n",
	instance->mc.obj.etx / RPL_DAG_MC_ETX_DIVISOR,
	(instance->mc.obj.etx % RPL_DAG_MC_ETX_DIVISOR * 100) / RPL_DAG_MC_ETX_DIVISOR);

#elif RPL_DAG_MC == RPL_DAG_MC_ENERGY

  instance->mc.type = RPL_DAG_MC_ENERGY;
  instance->mc.length = sizeof(instance->mc.obj.energy);

  if(dag->rank == ROOT_RANK(instance)) {
    type = RPL_DAG_MC_ENERGY_TYPE_MAINS;
  }

  instance->mc.obj.energy.flags = type << RPL_DAG_MC_ENERGY_TYPE;
  instance->mc.obj.energy.energy_est = path_metric;

#else

#error "Unsupported RPL_DAG_MC configured. See rpl.h."

#endif /* RPL_DAG_MC */
}
