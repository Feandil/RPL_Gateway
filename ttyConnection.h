#ifndef __TTY_CONNECTION_H__
#define __TTY_CONNECTION_H__

#include <stdint.h>
#include "sys/event.h"

#define TTY_SPEED B115200
#define TTY_BUFF_SIZE 30000
#define TTY_TEMP_BUFF_SIZE 8

#define SLIP_END     0300
#define SLIP_ESC     0333
#define SLIP_ESC_END 0334
#define SLIP_ESC_ESC 0335

enum {
  STATE_OK = 1,
  STATE_ESC = 2,
  STATE_RUBBISH = 3,
};

typedef struct slip_io_t {
  struct ev_io slip_ev;
  int fd;  /* FIle Descriptor */
  uint8_t buffer[TTY_BUFF_SIZE];
  uint8_t temp[TTY_TEMP_BUFF_SIZE];
  int read;
  int buffer_start;
  int buffer_end;
  uint8_t state;
  uint8_t first_end;
} slip_io_t;

int init_ttyUSBX(int X);

void tty_output(uint8_t *ptr, int size);

#endif /* __TTY_CONNECTION_H__ */


