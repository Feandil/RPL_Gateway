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
 * $Id: uip.h,v 1.35 2010/10/19 18:29:04 adamdunkels Exp $
 *
 */

#ifndef __UIP_H__
#define __UIP_H__

#include "uipopt.h"

/**
 * Representation of an IP address.
 *
 */
typedef union uip_ip6addr_t {
  u8_t  u8[16];			/* Initializer, must come first!!! */
  u16_t u16[8];
} uip_ip6addr_t;

typedef uip_ip6addr_t uip_ipaddr_t;

/*---------------------------------------------------------------------------*/

typedef struct uip_lladdr_t {
  u8_t addr[8];
} uip_lladdr_t;

#include "tcpip.h"
#define uip_sethostaddr(addr) uip_ipaddr_copy(&uip_hostaddr, (addr))
#define uip_gethostaddr(addr) uip_ipaddr_copy((addr), &uip_hostaddr)
#define uip_setdraddr(addr) uip_ipaddr_copy(&uip_draddr, (addr))
#define uip_setnetmask(addr) uip_ipaddr_copy(&uip_netmask, (addr))
#define uip_getdraddr(addr) uip_ipaddr_copy((addr), &uip_draddr)
#define uip_getnetmask(addr) uip_ipaddr_copy((addr), &uip_netmask)
void uip_setipid(u16_t id);
#define uip_input()        uip_process()

typedef union {
  uint32_t u32[(UIP_BUFSIZE + 3) / 4];
  uint8_t u8[UIP_BUFSIZE];
} uip_buf_t;

extern uip_buf_t uip_aligned_buf;
#define uip_buf (uip_aligned_buf.u8)

extern uip_lladdr_t uip_lladdr;
#define UIP_LLADDR_LEN 8

#define uip_ip6addr(addr, addr0,addr1,addr2,addr3,addr4,addr5,addr6,addr7) do { \
    (addr)->u16[0] = UIP_HTONS(addr0);                                      \
    (addr)->u16[1] = UIP_HTONS(addr1);                                      \
    (addr)->u16[2] = UIP_HTONS(addr2);                                      \
    (addr)->u16[3] = UIP_HTONS(addr3);                                      \
    (addr)->u16[4] = UIP_HTONS(addr4);                                      \
    (addr)->u16[5] = UIP_HTONS(addr5);                                      \
    (addr)->u16[6] = UIP_HTONS(addr6);                                      \
    (addr)->u16[7] = UIP_HTONS(addr7);                                      \
  } while(0)

#define uip_ip6addr_u8(addr, addr0,addr1,addr2,addr3,addr4,addr5,addr6,addr7,addr8,addr9,addr10,addr11,addr12,addr13,addr14,addr15) do { \
    (addr)->u8[0] = addr0;                                       \
    (addr)->u8[1] = addr1;                                       \
    (addr)->u8[2] = addr2;                                       \
    (addr)->u8[3] = addr3;                                       \
    (addr)->u8[4] = addr4;                                       \
    (addr)->u8[5] = addr5;                                       \
    (addr)->u8[6] = addr6;                                       \
    (addr)->u8[7] = addr7;                                       \
    (addr)->u8[8] = addr8;                                       \
    (addr)->u8[9] = addr9;                                       \
    (addr)->u8[10] = addr10;                                     \
    (addr)->u8[11] = addr11;                                     \
    (addr)->u8[12] = addr12;                                     \
    (addr)->u8[13] = addr13;                                     \
    (addr)->u8[14] = addr14;                                     \
    (addr)->u8[15] = addr15;                                     \
  } while(0)


#ifndef uip_ipaddr_copy
#define uip_ipaddr_copy(dest, src) (*(dest) = *(src))
#endif

#define uip_ipaddr_cmp(addr1, addr2) (memcmp(addr1, addr2, sizeof(uip_ip6addr_t)) == 0)
#define uip_ipaddr_prefixcmp(addr1, addr2, length) (memcmp(addr1, addr2, length>>3) == 0)

#ifndef UIP_HTONS
#   if UIP_BYTE_ORDER == UIP_BIG_ENDIAN
#      define UIP_HTONS(n) (n)
#      define UIP_HTONL(n) (n)
#   else /* UIP_BYTE_ORDER == UIP_BIG_ENDIAN */
#      define UIP_HTONS(n) (u16_t)((((u16_t) (n)) << 8) | (((u16_t) (n)) >> 8))
#      define UIP_HTONL(n) (((u32_t)UIP_HTONS(n) << 16) | UIP_HTONS((u32_t)(n) >> 16))
#   endif /* UIP_BYTE_ORDER == UIP_BIG_ENDIAN */
#else
#error "UIP_HTONS already defined!"
#endif /* UIP_HTONS */

#ifndef uip_htons
u16_t uip_htons(u16_t val);
#endif /* uip_htons */
#ifndef uip_ntohs
#define uip_ntohs uip_htons
#endif

#ifndef uip_htonl
u32_t uip_htonl(u32_t val);
#endif /* uip_htonl */
#ifndef uip_ntohl
#define uip_ntohl uip_htonl
#endif

extern u16_t uip_len;

extern u8_t uip_ext_len;

void uip_process(void);


/* The ICMP and IP headers. */
struct uip_icmpip_hdr {
  /* IPv6 header. */
  u8_t vtc,
    tcf;
  u16_t flow;
  u8_t len[2];
  u8_t proto, ttl;
  uip_ip6addr_t srcipaddr, destipaddr;
  u8_t type, icode;
  u16_t icmpchksum;
};

struct uip_ip_hdr {
  /* IPV6 header */
  u8_t vtc;
  u8_t tcflow;
  u16_t flow;
  u8_t len[2];
  u8_t proto, ttl;
  uip_ip6addr_t srcipaddr, destipaddr;
};

typedef struct uip_ext_hdr {
  u8_t next;
  u8_t len;
} uip_ext_hdr;

/* Hop by Hop option header */
typedef struct uip_hbho_hdr {
  u8_t next;
  u8_t len;
} uip_hbho_hdr;

/* destination option header */
typedef struct uip_desto_hdr {
  u8_t next;
  u8_t len;
} uip_desto_hdr;

typedef struct uip_routing_hdr {
  u8_t next;
  u8_t len;
  u8_t routing_type;
  u8_t seg_left;
} uip_routing_hdr;

typedef struct uip_ext_hdr_opt {
  u8_t type;
  u8_t len;
} uip_ext_hdr_opt;

/* PADN option */
typedef struct uip_ext_hdr_opt_padn {
  u8_t opt_type;
  u8_t opt_len;
} uip_ext_hdr_opt_padn;

/* RPL option */
typedef struct uip_ext_hdr_opt_rpl {
  u8_t opt_type;
  u8_t opt_len;
  u8_t flags;
  u8_t instance;
  u16_t senderrank;
} uip_ext_hdr_opt_rpl;

/* The ICMP headers. */
struct uip_icmp_hdr {
  u8_t type, icode;
  u16_t icmpchksum;
};

#define UIP_PROTO_ICMP  1
#define UIP_PROTO_TCP   6
#define UIP_PROTO_UDP   17
#define UIP_PROTO_ICMP6 58


#define UIP_PROTO_HBHO        0
#define UIP_PROTO_DESTO       60
#define UIP_PROTO_ROUTING     43
#define UIP_PROTO_FRAG        44
#define UIP_PROTO_NONE        59

#define UIP_EXT_HDR_OPT_PAD1  0
#define UIP_EXT_HDR_OPT_PADN  1
#define UIP_EXT_HDR_OPT_RPL   0x63

#define UIP_EXT_HDR_BITMAP_HBHO 0x01
#define UIP_EXT_HDR_BITMAP_DESTO1 0x02
#define UIP_EXT_HDR_BITMAP_ROUTING 0x04
#define UIP_EXT_HDR_BITMAP_FRAG 0x08
#define UIP_EXT_HDR_BITMAP_AH 0x10
#define UIP_EXT_HDR_BITMAP_ESP 0x20
#define UIP_EXT_HDR_BITMAP_DESTO2 0x40
/** @} */


#define UIP_IPH_LEN    40
#define UIP_FRAGH_LEN  8

#define UIP_UDPH_LEN    8    /* Size of UDP header */
#define UIP_TCPH_LEN   20    /* Size of TCP header */
#define UIP_ICMPH_LEN   4    /* Size of ICMP header */
#define UIP_IPUDPH_LEN (UIP_UDPH_LEN + UIP_IPH_LEN)    /* Size of IP +
                        * UDP
							   * header */
#define UIP_IPTCPH_LEN (UIP_TCPH_LEN + UIP_IPH_LEN)    /* Size of IP +
							   * TCP
							   * header */
#define UIP_TCPIP_HLEN UIP_IPTCPH_LEN
#define UIP_IPICMPH_LEN (UIP_IPH_LEN + UIP_ICMPH_LEN) /* size of ICMP
                                                         + IP header */
#define UIP_LLIPH_LEN (UIP_LLH_LEN + UIP_IPH_LEN)    /* size of L2
                                                        + IP header */
#define uip_l2_l3_hdr_len (UIP_LLH_LEN + UIP_IPH_LEN + uip_ext_len)
#define uip_l2_l3_icmp_hdr_len (UIP_LLH_LEN + UIP_IPH_LEN + uip_ext_len + UIP_ICMPH_LEN)
#define uip_l3_hdr_len (UIP_IPH_LEN + uip_ext_len)
#define uip_l3_icmp_hdr_len (UIP_IPH_LEN + uip_ext_len + UIP_ICMPH_LEN)


extern uip_ipaddr_t uip_hostaddr, uip_netmask, uip_draddr;
extern const uip_ipaddr_t uip_broadcast_addr;
extern const uip_ipaddr_t uip_all_zeroes_addr;

extern uip_lladdr_t uip_lladdr;

#define UIP_LLPREF_LEN     10

#define uip_is_addr_loopback(a)                  \
  ((((a)->u16[0]) == 0) &&                       \
   (((a)->u16[1]) == 0) &&                       \
   (((a)->u16[2]) == 0) &&                       \
   (((a)->u16[3]) == 0) &&                       \
   (((a)->u16[4]) == 0) &&                       \
   (((a)->u16[5]) == 0) &&                       \
   (((a)->u16[6]) == 0) &&                       \
   (((a)->u16[7]) == 1))

#define uip_is_addr_unspecified(a)               \
  ((((a)->u16[0]) == 0) &&                       \
   (((a)->u16[1]) == 0) &&                       \
   (((a)->u16[2]) == 0) &&                       \
   (((a)->u16[3]) == 0) &&                       \
   (((a)->u16[4]) == 0) &&                       \
   (((a)->u16[5]) == 0) &&                       \
   (((a)->u16[6]) == 0) &&                       \
   (((a)->u16[7]) == 0))

#define uip_is_addr_linklocal_allnodes_mcast(a)     \
  ((((a)->u8[0]) == 0xff) &&                        \
   (((a)->u8[1]) == 0x02) &&                        \
   (((a)->u16[1]) == 0) &&                          \
   (((a)->u16[2]) == 0) &&                          \
   (((a)->u16[3]) == 0) &&                          \
   (((a)->u16[4]) == 0) &&                          \
   (((a)->u16[5]) == 0) &&                          \
   (((a)->u16[6]) == 0) &&                          \
   (((a)->u8[14]) == 0) &&                          \
   (((a)->u8[15]) == 0x01))

#define uip_is_addr_linklocal_allrouters_mcast(a)     \
  ((((a)->u8[0]) == 0xff) &&                        \
   (((a)->u8[1]) == 0x02) &&                        \
   (((a)->u16[1]) == 0) &&                          \
   (((a)->u16[2]) == 0) &&                          \
   (((a)->u16[3]) == 0) &&                          \
   (((a)->u16[4]) == 0) &&                          \
   (((a)->u16[5]) == 0) &&                          \
   (((a)->u16[6]) == 0) &&                          \
   (((a)->u8[14]) == 0) &&                          \
   (((a)->u8[15]) == 0x02))

#define uip_is_addr_linklocal(a)                 \
  ((a)->u8[0] == 0xfe &&                         \
   (a)->u8[1] == 0x80)

#define uip_create_unspecified(a) uip_ip6addr(a, 0, 0, 0, 0, 0, 0, 0, 0)

#define uip_create_linklocal_allnodes_mcast(a) uip_ip6addr(a, 0xff02, 0, 0, 0, 0, 0, 0, 0x0001)

#define uip_create_linklocal_allrouters_mcast(a) uip_ip6addr(a, 0xff02, 0, 0, 0, 0, 0, 0, 0x0002)
#define uip_create_linklocal_prefix(addr) do { \
    (addr)->u16[0] = UIP_HTONS(0xfe80);            \
    (addr)->u16[1] = 0;                        \
    (addr)->u16[2] = 0;                        \
    (addr)->u16[3] = 0;                        \
  } while(0)

#define uip_is_addr_solicited_node(a)          \
  ((((a)->u8[0])  == 0xFF) &&                  \
   (((a)->u8[1])  == 0x02) &&                  \
   (((a)->u16[1]) == 0x00) &&                  \
   (((a)->u16[2]) == 0x00) &&                  \
   (((a)->u16[3]) == 0x00) &&                  \
   (((a)->u16[4]) == 0x00) &&                  \
   (((a)->u8[10]) == 0x00) &&                  \
   (((a)->u8[11]) == 0x01) &&                  \
   (((a)->u8[12]) == 0xFF))

#define uip_create_solicited_node(a, b)    \
  (((b)->u8[0]) = 0xFF);                        \
  (((b)->u8[1]) = 0x02);                        \
  (((b)->u16[1]) = 0);                          \
  (((b)->u16[2]) = 0);                          \
  (((b)->u16[3]) = 0);                          \
  (((b)->u16[4]) = 0);                          \
  (((b)->u8[10]) = 0);                          \
  (((b)->u8[11]) = 0x01);                       \
  (((b)->u8[12]) = 0xFF);                       \
  (((b)->u8[13]) = ((a)->u8[13]));              \
  (((b)->u16[7]) = ((a)->u16[7]))

#define uip_is_addr_link_local(a) \
  ((((a)->u8[0]) == 0xFE) && \
  (((a)->u8[1]) == 0x80))

#define uip_is_addr_mcast(a)                    \
  (((a)->u8[0]) == 0xFF)

#define uip_is_mcast_group_id_all_nodes(a) \
  ((((a)->u16[1])  == 0) &&                 \
   (((a)->u16[2])  == 0) &&                 \
   (((a)->u16[3])  == 0) &&                 \
   (((a)->u16[4])  == 0) &&                 \
   (((a)->u16[5])  == 0) &&                 \
   (((a)->u16[6])  == 0) &&                 \
   (((a)->u8[14])  == 0) &&                 \
   (((a)->u8[15])  == 1))

#define uip_is_mcast_group_id_all_routers(a) \
  ((((a)->u16[1])  == 0) &&                 \
   (((a)->u16[2])  == 0) &&                 \
   (((a)->u16[3])  == 0) &&                 \
   (((a)->u16[4])  == 0) &&                 \
   (((a)->u16[5])  == 0) &&                 \
   (((a)->u16[6])  == 0) &&                 \
   (((a)->u8[14])  == 0) &&                 \
   (((a)->u8[15])  == 2))


#define uip_are_solicited_bytes_equal(a, b)             \
  ((((a)->u8[13])  == ((b)->u8[13])) &&                 \
   (((a)->u8[14])  == ((b)->u8[14])) &&                 \
   (((a)->u8[15])  == ((b)->u8[15])))

u16_t uip_chksum(u16_t *buf, u16_t len);

u16_t uip_ipchksum(void);

u16_t uip_icmp6chksum(void);

#endif /* __UIP_H__ */
