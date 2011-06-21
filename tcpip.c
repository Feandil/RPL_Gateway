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
 *
 * $Id: tcpip.c,v 1.30 2010/10/29 05:36:07 adamdunkels Exp $
 */
/**
 * \file
 *         Code for tunnelling uIP packets over the Rime mesh routing module
 *
 * \author  Adam Dunkels <adam@sics.se>\author
 * \author  Mathilde Durvy <mdurvy@cisco.com> (IPv6 related code)
 * \author  Julien Abeille <jabeille@cisco.com> (IPv6 related code)
 */
#include "tcpip.h"

#include "uip-ds6.h"
#include "uip-icmp6.h"
#include "ttyConnection.h"
#include "tun.h"

#define DEBUG 1
#include "uip-debug.h"

#define UIP_ICMP_BUF ((struct uip_icmp_hdr *)&uip_buf[UIP_LLIPH_LEN + uip_ext_len])
#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

void rpl_init(void);
int rpl_add_header_root(void);
void rpl_remove_header(void);
int rpl_matching_used_prefix(uip_ipaddr_t *prefix);

uint8_t routage_type;

u8_t
tcpip_output()
{
  switch(routage_type) {
    case ROUTAGE_LAN:
       tty_output(uip_buf,uip_len);
       break;
    case ROUTAGE_WAN:
       tun_output(uip_buf,uip_len);
       break;
    default:
       PRINTF("Unable to route");
       break;
  }
  return 1; //TODO : Handle errors ...
}

/*---------------------------------------------------------------------------*/
void
tcpip_input(void)
{
  routage_type=ROUTAGE_WAN;
  if(uip_len > 0) {
    uip_input();
    if(uip_len > 0) {
      tcpip_ipv6_output();
    }
  }
  uip_len = 0;
  uip_ext_len = 0;
}
/*---------------------------------------------------------------------------*/
void
tcpip_ipv6_output(void)
{
  if(uip_len == 0) {
    return;
  }

  if(uip_len > UIP_LINK_MTU) {
    uip_len = 0;
    return;
  }

  if(uip_is_addr_unspecified(&UIP_IP_BUF->destipaddr)){
    uip_len = 0;
    return;
  }

  if(!uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
    /* Next hop determination */
    if(uip_ds6_is_addr_onlink(&UIP_IP_BUF->destipaddr)){
      routage_type = ROUTAGE_LAN;
    } else {
      uip_ds6_route_t* locrt;
      locrt = uip_ds6_route_lookup(&UIP_IP_BUF->destipaddr);
      if(locrt == NULL) {
        if (rpl_matching_used_prefix(&UIP_IP_BUF->destipaddr)) {
          PRINTF("ROUTAGE : Ce noeud devrait être local mais n'existe pas\n");
          uip_icmp6_error_output(ICMP6_DST_UNREACH,ICMP6_DST_UNREACH_ADDR,0);
          tcpip_ipv6_output();
          return;
        }
        routage_type = ROUTAGE_WAN;
      } else {
        switch(locrt->state.learned_from) {
          case RPL_ROUTE_FROM_INTERNAL:
            routage_type = ROUTAGE_WAN;
            break;
          case RPL_ROUTE_FROM_6LBR:
            /* We do not support rerouting from the backups */
            return;
          default:
            routage_type = ROUTAGE_LAN;
            break;
        }
      }
    }
    switch(routage_type) {
      case ROUTAGE_LAN:
         rpl_add_header_root();
         break;
      case ROUTAGE_WAN:
         rpl_remove_header();
         break;
      default:
         PRINTF("Unable to route");
         break;
    }
    tcpip_output();
  } else {
     routage_type = ROUTAGE_LAN;
     tcpip_output();
  }
}
