#ifndef __MOBILITY_H__
#define __MOBILITY_H__

#include <stdint.h>
#include "uip6.h"

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
} mob_bind_ack;

typedef struct mob_new_lbr {
  uint16_t reserved;
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
#define MOB_HDR_LEN 3

#define MOB_OPT_PREFIX    22
#define MOB_OPT_STATUS    42
#define MOB_OPT_PREF      23
#define MOB_OPT_NIO       8
#define MOB_OPT_TIMESTAMP 27

#define MOB_LEN_HDR      4
#define MOB_LEN_BIND     6
#define MOB_LEN_HANDOFF  4
#define MOB_LEN_NIO      12

#define MOB_HANDOFF_NEW_BINDING     1
#define MOB_HANDOFF_TWO_INTERFACES  2
#define MOB_HANDOFF_TWO_LBR         3
#define MOB_HANDOFF_UNKNOWN         4
#define MOB_HANDOFF_NO_CHANGE       5

#define MOB_HR_UPDATE   5
#define MOB_HR_ACK      6
#define MOB_HR_NEW_G    8

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

#define MOB_STATUS_ERR_FLAG 0x80

#endif /* __MOBILITY_H__ */
