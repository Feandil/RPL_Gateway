/**
 * \addtogroup uip6
 * @{
 */

/**
 * \file
 *         The uIP TCP/IPv6 stack code.
 *
 * \author Adam Dunkels <adam@sics.se>
 * \author Julien Abeille <jabeille@cisco.com> (IPv6 related code)
 * \author Mathilde Durvy <mdurvy@cisco.com> (IPv6 related code)
 */
/*
 * Copyright (c) 2001-2003, Adam Dunkels.
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: uip6.c,v 1.25 2011/01/04 22:11:37 joxe Exp $
 *
 */

/*
 * uIP is a small implementation of the IP, UDP and TCP protocols (as
 * well as some basic ICMP stuff). The implementation couples the IP,
 * UDP, TCP and the application layers very tightly. To keep the size
 * of the compiled code down, this code frequently uses the goto
 * statement. While it would be possible to break the uip_process()
 * function into many smaller functions, this would increase the code
 * size because of the overhead of parameter passing and the fact that
 * the optimier would not be as efficient.
 *
 * The principle is that we have a small buffer, called the uip_buf,
 * in which the device driver puts an incoming packet. The TCP/IP
 * stack parses the headers in the packet, and calls the
 * application. If the remote host has sent data to the application,
 * this data is present in the uip_buf and the application read the
 * data from there. It is up to the application to put this data into
 * a byte stream if needed. The application will not be fed with data
 * that is out of sequence.
 *
 * If the application whishes to send data to the peer, it should put
 * its data into the uip_buf. The uip_appdata pointer points to the
 * first available byte. The TCP/IP stack will calculate the
 * checksums, and fill in the necessary header fields and finally send
 * the packet back to the peer.
 */

#include "uip6.h"
#include "uipopt.h"
#include "uip-icmp6.h"
#include "uip-ds6.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* For Debug, logging, statistics                                            */
/*---------------------------------------------------------------------------*/

#define DEBUG DEBUG_NONE
#include "uip-debug.h"

void uip_rpl_input(void);
int rpl_verify_header(int uip_ext_opt_offset);

uip_lladdr_t uip_lladdr = {{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x42}};
/** @} */

/*---------------------------------------------------------------------------*/
/** @{ \name Layer 3 variables */
/*---------------------------------------------------------------------------*/
/**
 * \brief Type of the next header in IPv6 header or extension headers
 *
 * Can be the next header field in the IPv6 header or in an extension header.
 * When doing fragment reassembly, we must change the value of the next header
 * field in the header before the fragmentation header, hence we need a pointer
 * to this field.
 */
u8_t *uip_next_hdr;
/** \brief bitmap we use to record which IPv6 headers we have already seen */
u8_t uip_ext_bitmap = 0;
/**
 * \brief length of the extension headers read. updated each time we process
 * a header
 */
u8_t uip_ext_len = 0;
/** \brief length of the header options read */
u8_t uip_ext_opt_offset = 0;
/** @} */

/*---------------------------------------------------------------------------*/
/* Buffers                                                                   */
/*---------------------------------------------------------------------------*/
/** \name Buffer defines
 *  @{
 */
#define FBUF                             ((struct uip_tcpip_hdr *)&uip_reassbuf[0])
#define UIP_IP_BUF                          ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF                      ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_UDP_BUF                        ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_TCP_BUF                        ((struct uip_tcp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_EXT_BUF                        ((struct uip_ext_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_ROUTING_BUF                ((struct uip_routing_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_FRAG_BUF                      ((struct uip_frag_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_HBHO_BUF                      ((struct uip_hbho_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_DESTO_BUF                    ((struct uip_desto_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_EXT_HDR_OPT_BUF            ((struct uip_ext_hdr_opt *)&uip_buf[uip_l2_l3_hdr_len + uip_ext_opt_offset])
#define UIP_EXT_HDR_OPT_PADN_BUF  ((struct uip_ext_hdr_opt_padn *)&uip_buf[uip_l2_l3_hdr_len + uip_ext_opt_offset])
#define UIP_EXT_HDR_OPT_RPL_BUF    ((struct uip_ext_hdr_opt_rpl *)&uip_buf[uip_l2_l3_hdr_len + uip_ext_opt_offset])
#define UIP_ICMP6_ERROR_BUF            ((struct uip_icmp6_error *)&uip_buf[uip_l2_l3_icmp_hdr_len])
/** @} */
/** \name Buffer variables
 *  @{
 */
/** Packet buffer for incoming and outgoing packets */
#ifndef UIP_CONF_EXTERNAL_BUFFER
uip_buf_t uip_aligned_buf;
#endif /* UIP_CONF_EXTERNAL_BUFFER */


/* The uip_len is either 8 or 16 bits, depending on the maximum packet size.*/
u16_t uip_len, uip_slen;
/** @} */

/*---------------------------------------------------------------------------*/
static u16_t
chksum(u16_t sum, const u8_t *data, u16_t len)
{
  u16_t t;
  const u8_t *dataptr;
  const u8_t *last_byte;

  dataptr = data;
  last_byte = data + len - 1;
  
  while(dataptr < last_byte) {   /* At least two more bytes */
    t = (dataptr[0] << 8) + dataptr[1];
    sum += t;
    if(sum < t) {
      sum++;      /* carry */
    }
    dataptr += 2;
  }
  
  if(dataptr == last_byte) {
    t = (dataptr[0] << 8) + 0;
    sum += t;
    if(sum < t) {
      sum++;      /* carry */
    }
  }

  /* Return sum in host byte order. */
  return sum;
}
/*---------------------------------------------------------------------------*/
u16_t
uip_chksum(u16_t *data, u16_t len)
{
  return uip_htons(chksum(0, (u8_t *)data, len));
}
/*---------------------------------------------------------------------------*/
u16_t
uip_ipchksum(void)
{
  u16_t sum;

  sum = chksum(0, &uip_buf[UIP_LLH_LEN], UIP_IPH_LEN);
  PRINTF("uip_ipchksum: sum 0x%04x\n", sum);
  return (sum == 0) ? 0xffff : uip_htons(sum);
}
/*---------------------------------------------------------------------------*/
static u16_t
upper_layer_chksum(u8_t proto)
{
  u16_t upper_layer_len;
  u16_t sum;
  
  upper_layer_len = (((u16_t)(UIP_IP_BUF->len[0]) << 8) + UIP_IP_BUF->len[1] - uip_ext_len) ;
  
  /* First sum pseudoheader. */
  /* IP protocol and length fields. This addition cannot carry. */
  sum = upper_layer_len + proto;
  /* Sum IP source and destination addresses. */
  sum = chksum(sum, (u8_t *)&UIP_IP_BUF->srcipaddr, 2 * sizeof(uip_ipaddr_t));

  /* Sum TCP header and data. */
  sum = chksum(sum, &uip_buf[UIP_IPH_LEN + UIP_LLH_LEN + uip_ext_len],
               upper_layer_len);
    
  return (sum == 0) ? 0xffff : uip_htons(sum);
}
/*---------------------------------------------------------------------------*/
u16_t
uip_icmp6chksum(void)
{
  return upper_layer_chksum(UIP_PROTO_ICMP6);
  
}
/*---------------------------------------------------------------------------*/
void
uip_init(void)
{
  uip_ds6_init();
}

/*---------------------------------------------------------------------------*/

/**
 * \brief Process the options in Destination and Hop By Hop extension headers
 */
static u8_t
ext_hdr_options_process() {
 /*
  * Length field in the extension header: length of th eheader in units of
  * 8 bytes, excluding the first 8 bytes
  * length field in an option : the length of data in the option
  */
  uip_ext_opt_offset = 2;
  while(uip_ext_opt_offset  < ((UIP_EXT_BUF->len << 3) + 8)) {
    switch (UIP_EXT_HDR_OPT_BUF->type) {
      /*
       * for now we do not support any options except padding ones
       * PAD1 does not make sense as the header must be 8bytes aligned,
       * hence we can only have
       */
      case UIP_EXT_HDR_OPT_PAD1:
        PRINTF("Processing PAD1 option\n");
        uip_ext_opt_offset += 1;
        break;
      case UIP_EXT_HDR_OPT_PADN:
        PRINTF("Processing PADN option\n");
        uip_ext_opt_offset += UIP_EXT_HDR_OPT_PADN_BUF->opt_len + 2;
        break;
      case UIP_EXT_HDR_OPT_RPL:
        PRINTF("Processing RPL option\n");
        if (rpl_verify_header(uip_ext_opt_offset)) {
          PRINTF("RPL Option Error : Dropping Packet");
          return 1;
        }
        uip_ext_opt_offset += (UIP_EXT_HDR_OPT_RPL_BUF->opt_len) + 2;
        return 0;
      default:
        /*
         * check the two highest order bits of the option
         * - 00 skip over this option and continue processing the header.
         * - 01 discard the packet.
         * - 10 discard the packet and, regardless of whether or not the
         *   packet's Destination Address was a multicast address, send an
         *   ICMP Parameter Problem, Code 2, message to the packet's
         *   Source Address, pointing to the unrecognized Option Type.
         * - 11 discard the packet and, only if the packet's Destination
         *   Address was not a multicast address, send an ICMP Parameter
         *   Problem, Code 2, message to the packet's Source Address,
         *   pointing to the unrecognized Option Type.
         */
        PRINTF("MSB %x\n", UIP_EXT_HDR_OPT_BUF->type);
        switch(UIP_EXT_HDR_OPT_BUF->type & 0xC0) {
          case 0:
            break;
          case 0x40:
            return 1;
          case 0xC0:
            if(uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
              return 1;
            }
          case 0x80:
            uip_icmp6_error_output(ICMP6_PARAM_PROB, ICMP6_PARAMPROB_OPTION,
                             (u32_t)UIP_IPH_LEN + uip_ext_len + uip_ext_opt_offset);
            return 2;
        }
        /* in the cases were we did not discard, update ext_opt* */
        uip_ext_opt_offset += UIP_EXT_HDR_OPT_BUF->len + 2;
        break;
    }
  }
  return 0;
}


/*---------------------------------------------------------------------------*/
void
uip_process()
{
  PRINTF("NEW packet\n");

  /* Check validity of the IP header. */
  if((UIP_IP_BUF->vtc & 0xf0) != 0x60)  { /* IP version and header length. */
    PRINTF("Wrong IP Version\n");
    goto drop;
  }
  /*
   * Check the size of the packet. If the size reported to us in
   * uip_len is smaller the size reported in the IP header, we assume
   * that the packet has been corrupted in transit. If the size of
   * uip_len is larger than the size reported in the IP packet header,
   * the packet has been padded and we set uip_len to the correct
   * value..
   */

  if((UIP_IP_BUF->len[0] << 8) + UIP_IP_BUF->len[1] <= uip_len) {
    uip_len = (UIP_IP_BUF->len[0] << 8) + UIP_IP_BUF->len[1] + UIP_IPH_LEN;
    /*
     * The length reported in the IPv6 header is the
     * length of the payload that follows the
     * header. However, uIP uses the uip_len variable
     * for holding the size of the entire packet,
     * including the IP header. For IPv4 this is not a
     * problem as the length field in the IPv4 header
     * contains the length of the entire packet. But
     * for IPv6 we need to add the size of the IPv6
     * header (40 bytes).
     */
  } else {
    PRINTF("Wrong Length\n");
    goto drop;
  }
  
  PRINTF("IPv6 packet received from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF(" to ");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF("\n");

  if(uip_is_addr_mcast(&UIP_IP_BUF->srcipaddr)){
    PRINTF("Dropping packet, src is mcast\n");
    goto drop;
  }

  /*
   * Next header field processing. In IPv6, we can have extension headers,
   * if present, the Hop-by-Hop Option must be processed before forwarding
   * the packet.
   */
  uip_next_hdr = &UIP_IP_BUF->proto;
  uip_ext_len = 0;
  uip_ext_bitmap = 0;
  if (*uip_next_hdr == UIP_PROTO_HBHO) {
#if UIP_CONF_IPV6_CHECKS
    uip_ext_bitmap |= UIP_EXT_HDR_BITMAP_HBHO;
#endif /*UIP_CONF_IPV6_CHECKS*/
    switch(ext_hdr_options_process()) {
      case 0:
        /*continue*/
        uip_next_hdr = &UIP_EXT_BUF->next;
        uip_ext_len += (UIP_EXT_BUF->len << 3) + 8;
        break;
      case 1:
        /*silently discard*/
        goto drop;
      case 2:
        /* send icmp error message (created in ext_hdr_options_process)
         * and discard*/
        goto send;
    }
  }


  /* TBD Some Parameter problem messages */
  if(!uip_ds6_is_my_addr(&UIP_IP_BUF->destipaddr) &&
     !uip_ds6_is_my_maddr(&UIP_IP_BUF->destipaddr)) {
    if(!uip_is_addr_mcast(&UIP_IP_BUF->destipaddr) &&
       !uip_is_addr_link_local(&UIP_IP_BUF->destipaddr) &&
       !uip_is_addr_link_local(&UIP_IP_BUF->srcipaddr) &&
       !uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr) &&
       !uip_is_addr_loopback(&UIP_IP_BUF->destipaddr)) {


      /* Check MTU */
      if(uip_len > UIP_LINK_MTU) {
        uip_icmp6_error_output(ICMP6_PACKET_TOO_BIG, 0, UIP_LINK_MTU);
        goto send;
      }
      /* Check Hop Limit */
      if(UIP_IP_BUF->ttl <= 1) {
        uip_icmp6_error_output(ICMP6_TIME_EXCEEDED,
                               ICMP6_TIME_EXCEED_TRANSIT, 0);
        goto send;
      }

      UIP_IP_BUF->ttl = UIP_IP_BUF->ttl - 1;
      PRINTF("Forwarding packet to ");
      PRINT6ADDR(&UIP_IP_BUF->destipaddr);
      PRINTF("\n");
      goto send;
    } else {
      if((uip_is_addr_link_local(&UIP_IP_BUF->srcipaddr)) &&
         (!uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr)) &&
         (!uip_is_addr_loopback(&UIP_IP_BUF->destipaddr)) &&
         (!uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) &&
         (!uip_ds6_is_addr_onlink((&UIP_IP_BUF->destipaddr)))) {
        PRINTF("LL source address with off link destination, dropping\n");
        uip_icmp6_error_output(ICMP6_DST_UNREACH,
                               ICMP6_DST_UNREACH_NOTNEIGHBOR, 0);
        goto send;
      }
      PRINTF("Dropping packet, not for me and link local or multicast\n");
      goto drop;
    }
  }

  while(1) {
    switch(*uip_next_hdr){
      case UIP_PROTO_ICMP6:
        /* ICMPv6 */
        goto icmp6_input;
      case UIP_PROTO_HBHO:
        PRINTF("Processing hbh header\n");
        /* Hop by hop option header */
#if UIP_CONF_IPV6_CHECKS
        /* Hop by hop option header. If we saw one HBH already, drop */
        if(uip_ext_bitmap & UIP_EXT_HDR_BITMAP_HBHO) {
          goto bad_hdr;
        } else {
          uip_ext_bitmap |= UIP_EXT_HDR_BITMAP_HBHO;
        }
#endif /*UIP_CONF_IPV6_CHECKS*/
        switch(ext_hdr_options_process()) {
          case 0:
            /*continue*/
            uip_next_hdr = &UIP_EXT_BUF->next;
            uip_ext_len += (UIP_EXT_BUF->len << 3) + 8;
            break;
          case 1:
            /*silently discard*/
            goto drop;
          case 2:
            /* send icmp error message (created in ext_hdr_options_process)
             * and discard*/
            goto send;
        }
        break;
      case UIP_PROTO_DESTO:
#if UIP_CONF_IPV6_CHECKS
        /* Destination option header. if we saw two already, drop */
        PRINTF("Processing desto header\n");
        if(uip_ext_bitmap & UIP_EXT_HDR_BITMAP_DESTO1) {
          if(uip_ext_bitmap & UIP_EXT_HDR_BITMAP_DESTO2) {
            goto bad_hdr;
          } else{
            uip_ext_bitmap |= UIP_EXT_HDR_BITMAP_DESTO2;
          }
        } else {
          uip_ext_bitmap |= UIP_EXT_HDR_BITMAP_DESTO1;
        }
#endif /*UIP_CONF_IPV6_CHECKS*/
        switch(ext_hdr_options_process()) {
          case 0:
            /*continue*/
            uip_next_hdr = &UIP_EXT_BUF->next;
            uip_ext_len += (UIP_EXT_BUF->len << 3) + 8;
            break;
          case 1:
            /*silently discard*/
            goto drop;
          case 2:
            /* send icmp error message (created in ext_hdr_options_process)
             * and discard*/
            goto send;
        }
        break;
      case UIP_PROTO_ROUTING:
#if UIP_CONF_IPV6_CHECKS
        /* Routing header. If we saw one already, drop */
        if(uip_ext_bitmap & UIP_EXT_HDR_BITMAP_ROUTING) {
          goto bad_hdr;
        } else {
          uip_ext_bitmap |= UIP_EXT_HDR_BITMAP_ROUTING;
        }
#endif /*UIP_CONF_IPV6_CHECKS*/
        /*
         * Routing Header  length field is in units of 8 bytes, excluding
         * As per RFC2460 section 4.4, if routing type is unrecognized:
         * if segments left = 0, ignore the header
         * if segments left > 0, discard packet and send icmp error pointing
         * to the routing type
         */

        PRINTF("Processing Routing header\n");
        if(UIP_ROUTING_BUF->seg_left > 0) {
          uip_icmp6_error_output(ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER, UIP_IPH_LEN + uip_ext_len + 2);
          goto send;
        }
        uip_next_hdr = &UIP_EXT_BUF->next;
        uip_ext_len += (UIP_EXT_BUF->len << 3) + 8;
        break;
      case UIP_PROTO_FRAG:
        /* Fragmentation header:call the reassembly function, then leave */
        goto drop;
      case UIP_PROTO_NONE:
        goto drop;
      default:
        goto bad_hdr;
    }
  }
  bad_hdr:
  /*
   * RFC 2460 send error message parameterr problem, code unrecognized
   * next header, pointing to the next header field
   */
  uip_icmp6_error_output(ICMP6_PARAM_PROB, ICMP6_PARAMPROB_NEXTHEADER, (u32_t)(uip_next_hdr - (uint8_t *)UIP_IP_BUF));
  goto send;
  /* End of headers processing */
  
  icmp6_input:
  /* This is IPv6 ICMPv6 processing code. */
  PRINTF("icmp6_input: length %d\n", uip_len);

#if UIP_CONF_IPV6_CHECKS
  /* Compute and check the ICMP header checksum */
  if(uip_icmp6chksum() != 0xffff) {
    goto drop;
  }
#endif /*UIP_CONF_IPV6_CHECKS*/

  /*
   * Here we process incoming ICMPv6 packets
   * For echo request, we send echo reply
   * For ND pkts, we call the appropriate function in uip-nd6.c
   * We do not treat Error messages for now
   * If no pkt is to be sent as an answer to the incoming one, we
   * "goto drop". Else we just break; then at the after the "switch"
   * we "goto send"
   */
  switch(UIP_ICMP_BUF->type) {
    case ICMP6_NS:
      uip_len = 0;
      break;
    case ICMP6_NA:
      uip_len = 0;
      break;
    case ICMP6_RS:
      uip_len = 0;
      break;
    case ICMP6_RA:
      uip_len = 0;
      break;
    case ICMP6_RPL:
      uip_rpl_input();
      break;
    case ICMP6_ECHO_REQUEST:
      uip_icmp6_echo_request_input();
      break;
    case ICMP6_ECHO_REPLY:
      /** \note We don't implement any application callback for now */
      PRINTF("Received an icmp6 echo reply\n");
      uip_len = 0;
      break;
    default:
      PRINTF("Unknown icmp6 message type %d\n", UIP_ICMP_BUF->type);
      uip_len = 0;
      break;
  }

  if(uip_len > 0) {
    goto send;
  } else {
    goto drop;
  }
  /* End of IPv6 ICMP processing. */

  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->tcflow = 0x00;
  UIP_IP_BUF->flow = 0x00;
 send:
  PRINTF("Sending packet with length %d (%d)\n", uip_len,
         (UIP_IP_BUF->len[0] << 8) | UIP_IP_BUF->len[1]);
  /* Return and let the caller do the actual transmission. */
  return;

 drop:
  uip_len = 0;
  uip_ext_len = 0;
  uip_ext_bitmap = 0;
  return;
}
/*---------------------------------------------------------------------------*/
u16_t
uip_htons(u16_t val)
{
  return UIP_HTONS(val);
}

u32_t
uip_htonl(u32_t val)
{
  return UIP_HTONL(val);
}
/*---------------------------------------------------------------------------*/
