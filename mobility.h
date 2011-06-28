#ifndef __MOBILITY_H__
#define __MOBILITY_H__

#include <stdint.h>
#include "uip6.h"
#include "uip-ds6.h"

typedef struct ext_hdr {
  uint8_t type;
  uint8_t len;
  uint8_t mess;
  uint8_t reserved;
  uint8_t next;
} ext_hdr;

typedef struct mob_bind_up {
  uint16_t sequence;
  uint8_t flag;
  uint8_t reserved;
  uint16_t lifetime;
  uint8_t options;
} mob_bind_up;

typedef struct mob_bind_ack {
  uint8_t status;
  uint8_t flag;
  uint16_t sequence;
  uint16_t lifetime;
  uint8_t options;
} mob_bind_ack;

typedef struct mob_new_lbr {
  uint8_t type;
  uint8_t flags;
  uip_ipaddr_t addr;
} mob_new_lbr;

typedef struct mob_opt {
  uint8_t type;
  uint8_t len;
} mob_opt;

typedef struct mob_opt_prefix {
  uint8_t type;
  uint8_t len;
  uint8_t reserved;
  uint8_t prefix_len;
  uip_ipaddr_t addr;
} mob_opt_prefix;

typedef struct mob_opt_status {
  uint8_t type;
  uint8_t len;
  uint8_t reserved;
  uint8_t status;
} mob_opt_status;

typedef struct mob_opt_pref {
  uint8_t type;
  uint8_t len;
  uint8_t pref;
  uint8_t hi;
} mob_opt_pref;

typedef struct mob_opt_nio {
  uint8_t type;
  uint8_t len;
  uint16_t reserved;
  uip_lladdr_t addr;
} mob_opt_nio;

typedef struct mob_opt_timestamp {
  uint8_t type;
  uint8_t len;
  uint8_t reserved;
  uint8_t status;
} mob_opt_timestamp;

#define MOB_TYPE  135
#define MOB_HDR_LEN 4
#define MOB_MAX_ENTRY_BY_UP 15

#define MOB_TYPE_UPWARD 0x01
#define MOB_TYPE_STORE 0x02
#define MOB_TYPE_APPLY 0x04

#define MOB_GW_KNOWN     1
#define MOB_GW_RESERVED  2

#define MOB_OPT_PREFIX    22
#define MOB_OPT_STATUS    42
#define MOB_OPT_PREF      23
#define MOB_OPT_NIO       8
#define MOB_OPT_TIMESTAMP 27

#define MOB_LEN_HDR      4
#define MOB_LEN_BIND     6
#define MOB_LEN_ACK      6
#define MOB_LEN_HANDOFF  4
#define MOB_LEN_STATUS   4
#define MOB_LEN_NIO      12

#define MOB_HANDOFF_NEW_BINDING     1
#define MOB_HANDOFF_TWO_INTERFACES  2
#define MOB_HANDOFF_TWO_LBR         3
#define MOB_HANDOFF_UNKNOWN         4
#define MOB_HANDOFF_NO_CHANGE       5

#define MOB_HR_UPDATE   5
#define MOB_HR_ACK      6
#define MOB_HR_NEW_G    8

#define MOB_LBR_PRI   0x80
#define MOB_LBR_BCK   0x40
#define MOB_LBR_LBR   0x00

#define MOB_FLAG_UP_A  0x80
#define MOB_FLAG_UP_H  0x40
#define MOB_FLAG_UP_L  0x20
#define MOB_FLAG_UP_K  0x10
#define MOB_FLAG_UP_M  0x08
#define MOB_FLAG_UP_R  0x04
#define MOB_FLAG_UP_P  0x02
#define MOB_FLAG_UP_O  0x01

#define MOB_FLAG_ACK_K  0x80
#define MOB_FLAG_ACK_R  0x40
#define MOB_FLAG_ACK_P  0x20
#define MOB_FLAG_ACK_O  0x10

#define MOB_FLAG_LBR_R  0x80
#define MOB_FLAG_LBR_S  0x40
#define MOB_FLAG_LBR_Q  0x20
#define MOB_FLAG_LBR_U  0x10

#define MOB_STATUS_ERR_FLAG 0x80

#define IP6_LEN  128
#define MAX_DEVNAME_SIZE     10
#define MAX_DEVNAME_NUM_SIZE 3
#define MAX_KNOWN_GATEWAY 15
#define MAX_DELETE_NIO 30
#define MAX_LBR_BACKUP 2

#define MOB_SEND_DELAY   2
#define MOB_SEND_TIMEOUT 10

typedef struct gw_list {
  uint16_t sequence_out;
//  uint16_t sequence_in;
  uint8_t used;
  uint8_t devnum;
  struct sockaddr_in6 hoag_addr;
} gw_list;

typedef struct uip_lladdr_list {
  uint8_t used;
  uint8_t seq;
  uip_lladdr_t addr;
} uip_lladdr_list;

void receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len);
int mob_init(uint8_t state, int p, uip_ipaddr_t *pre, uip_ipaddr_t *ip, char* devname, char* ttydev);
void hoag_init_lbr(uip_ip6addr_t *lbr);
void hoag_close_tunnels(void);

#endif /* __MOBILITY_H__ */
