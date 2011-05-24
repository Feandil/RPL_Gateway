/**
 * \addtogroup timestamp
 * @{
 */

/**
 * \file
 * Timestamp library implementation.
 */

/*
 * Copyright (c) 2004, Swedish Institute of Computer Science.
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
 * Author: Adam Dunkels <adam@sics.se>
 */

#include "sys/clock.h"
#include "sys/timestamp.h"

/*---------------------------------------------------------------------------*/
/**
 * Set a timestamp.
 *
 * This function is used to set a timestamp for a time sometime in the
 * future. The function timestamp_expired() will evaluate to true after
 * the timestamp has expired.
 *
 * \param t A pointer to the timestamp
 * \param interval The interval before the timestamp expires.
 *
 */
void
timestamp_set(struct timestamp *t, clock_time_t interval)
{
  t->end = clock_time() + interval;
}
/*---------------------------------------------------------------------------*/
/**
 * Reset the timestamp with the given interval.
 *
 * The start point of the interval is the exact time that the timestamp
 * last expired. Therefore, this function will cause the timestamp to be
 * stable over time, unlike the timestamp_restart() function.
 *
 * \param t A pointer to the timestamp.
 * \param interval The interval before the timestamp expires.
 *
 * \sa timestamp_restart()
 */
void
timestamp_reset(struct timestamp *t, clock_time_t interval)
{
  t->end += interval;
}
/*---------------------------------------------------------------------------*/
/**
 * Check if a timestamp has expired.
 *
 * This function tests if a timestamp has expired and returns true or
 * false depending on its status.
 *
 * \param t A pointer to the timestamp
 *
 * \return Non-zero if the timestamp has expired, zero otherwise.
 *
 */
int
timestamp_expired(struct timestamp *t)
{
  /* Note: Can not return clocktime <= t->end so we add 1 to diff and return
     t->end > clocktime - required to avoid an internal error in mspgcc. */
  clock_time_t clocktime = clock_time() + 1;
  return t->end > clocktime;

}
/*---------------------------------------------------------------------------*/
/**
 * The time until the timestamp expires
 *
 * This function returns the time until the timestamp expires.
 *
 * \param t A pointer to the timestamp
 *
 * \return The time until the timestamp expires
 *
 */
clock_time_t
timestamp_remaining(struct timestamp *t)
{
  return t->end - clock_time();
}
/*---------------------------------------------------------------------------*/

/** @} */
