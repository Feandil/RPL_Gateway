/** \addtogroup sys
 * @{ */

/**
 * \defgroup stimestamp Seconds timestamp library
 *
 * The stimestamp library provides functions for setting, resetting and
 * restarting timestampss, and for checking if a timestamp has expired. An
 * application must "manually" check if its timestamps have expired; this
 * is not done automatically.
 *
 * A timestamp is declared as a \c struct \c stimestamp and all access to the
 * timestamp is made by a pointer to the declared timestamp.
 *
 * \note The stimestamp library is not able to post events when a timestamp
 * expires. The \ref etimer "Event timers" should be used for this
 * purpose.
 *
 * \note The stimestamp library uses the \ref clock "Clock library" to
 * measure time. Intervals should be specified in the seconds.
 *
 * \sa \ref etimer "Event timers"
 *
 * @{
 */


/**
 * \file
 * Second timestamp library header file.
 */

/*
 * Copyright (c) 2004, 2008, Swedish Institute of Computer Science.
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
 * Author: Adam Dunkels <adam@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */

#ifndef __STIMESTAMP_H__
#define __STIMESTAMP_H__

#include "sys/clock.h"

/**
 * A timestamp.
 *
 * This structure is used for declaring a timestamp. The timestamp must be set
 * with stimestamp_set() before it can be used.
 *
 * \hideinitializer
 */
struct stimestamp {
  unsigned long end;
};

void stimestamp_set(struct stimestamp *t, unsigned long interval);
void stimestamp_reset(struct stimestamp *t, unsigned long interval);
int stimestamp_expired(struct stimestamp *t);
unsigned long stimestamp_remaining(struct stimestamp *t);


#endif /* __STIMESTAMP_H__ */

/** @} */
/** @} */
