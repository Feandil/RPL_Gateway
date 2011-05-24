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
 */
/**
 * \file
 *         RPL timer management.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */

#include "conf.h"
#include "rpl/rpl-private.h"
#include "lib/random.h"
#include "sys/event.h"
#include "sys/stimestamp.h"

#define DEBUG 1
#include "uip-debug.h"

/************************************************************************/
static void new_dio_interval(rpl_instance_t *instance);
static void handle_dio_timer(struct ev_loop *loop, struct ev_periodic *w, int revents);

/* dio_send_ok is true if the node is ready to send DIOs */
static uint8_t dio_send_ok;

/************************************************************************/
ev_tstamp dio_rescheduler(struct ev_periodic *w, ev_tstamp now)
{
  PRINTF("NEXT DELAY : %"PRIu32"\n",((rpl_ev_dio_t *)w)->dio_next_delay);
  return now + ((rpl_ev_dio_t *)w)->dio_next_delay;
}
/************************************************************************/
static void
new_dio_interval(rpl_instance_t *instance)
{
  unsigned long time;

  /* TODO: too small timer intervals for many cases */
  time = 1UL << instance->dio_intcurrent;

  /* Convert from milliseconds to second. */
  time = time  / 1000;

  instance->dio_timer.dio_next_delay = time;

  /* random number between I/2 and I */
  time = time >> 1;
  time += (time * random_rand()) / RANDOM_RAND_MAX;

  /*
   * The intervals must be equally long among the nodes for Trickle to
   * operate efficiently. Therefore we need to calculate the delay between
   * the randomized time and the start time of the next interval.
   */
  instance->dio_timer.dio_next_delay -= time;
  instance->dio_send = 1;

  /* reset the redundancy counter */
  instance->dio_counter = 0;

  /* schedule the timer */
  PRINTF("RPL: Scheduling DIO timer %lu ticks in future (Interval)\n", time);
  ev_periodic_again(event_loop,&instance->dio_timer.periodic);
}
/************************************************************************/
static void
handle_dio_timer(struct ev_loop *loop, struct ev_periodic *w, int revents)
{
  rpl_instance_t *instance;

  instance = ((rpl_ev_dio_t *)w)->instance;

  PRINTF("RPL: DIO Timer triggered\n");
  if(!dio_send_ok) {
    if(uip_ds6_get_link_local(ADDR_PREFERRED) != NULL) {
      dio_send_ok = 1;
    } else {
      PRINTF("RPL: Postponing DIO transmission since link local address is not ok\n");
      instance->dio_timer.dio_next_delay=CLOCK_SECOND;
      ev_periodic_again(event_loop,&instance->dio_timer.periodic);
      return;
    }
  }

  if(instance->dio_send) {
    /* send DIO if counter is less than desired redundancy */
    if(instance->dio_counter < instance->dio_redundancy) {
      dio_output(instance, NULL);
    } else {
      PRINTF("RPL: Supressing DIO transmission (%d >= %d)\n",
             instance->dio_counter, instance->dio_redundancy);
    }
    instance->dio_send = 0;
    PRINTF("RPL: Scheduling DIO timer %"PRIu32" ticks in future (sent)\n",
           instance->dio_timer.dio_next_delay);
     ev_periodic_again(event_loop,&instance->dio_timer.periodic);
  } else {
    /* check if we need to double interval */
    if(instance->dio_intcurrent < instance->dio_intmin + instance->dio_intdoubl) {
      instance->dio_intcurrent++;
      PRINTF("RPL: DIO Timer interval doubled %d\n", instance->dio_intcurrent);
    }
    new_dio_interval(instance);
  }
}
/************************************************************************/
/* Resets the DIO timer in the instance to its minimal interval. */
void
rpl_reset_dio_timer(rpl_instance_t *instance, uint8_t force)
{
  /* only reset if not just reset or started */
  if(force || instance->dio_intcurrent > instance->dio_intmin) {
    instance->dio_counter = 0;
    instance->dio_intcurrent = instance->dio_intmin;
    if (!ev_is_active(&instance->dio_timer.periodic)) {
      ev_periodic_init(&instance->dio_timer.periodic,handle_dio_timer,0,0,dio_rescheduler);
      ev_periodic_start(event_loop,&instance->dio_timer.periodic);
    }
    new_dio_interval(instance);
  }
}
/************************************************************************/
