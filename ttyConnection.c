
#include "ttyConnection.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "uip6.h"
#include "tcpip.h"

#define DEBUG DEBUG_NONE
#include "uip-debug.h"

unsigned char *slipend;
unsigned char *slipesc;
unsigned char *slipescend;
unsigned char *slipescesc;
struct slip_io_t *slip_io;

static void handle_input(slip_io_t *slip_io)
{
  unsigned char temp[64];
  int res;

#if DEBUG
  int j;
#endif

  switch(slip_io->buffer[slip_io->buffer_start]) {
    case '!':
      switch (slip_io->buffer[slip_io->buffer_start+1]) {
        case 'S':
        case 'P':
          temp[0]='!';
          temp[1]='P';
          temp[2]=0xfc;
          temp[3]=0x0;
          temp[4]=0x0;
          temp[5]=0x0;
          temp[6]=0x0;
          temp[7]=0x0;
          temp[8]=0x0;
          temp[9]=0x0;
          temp[10]=SLIP_END;
          res=write(slip_io->fd,temp,11);
          break;
        default:
          PRINTF("Unknown conctrol char : %c\n",slip_io->buffer[slip_io->buffer_start+1]);
          break;
      }
      break;
    case '`':
      if(slip_io->first_end) {
        slip_io->first_end=0;
      }
      if (slip_io->buffer_end>slip_io->buffer_start) {
        PRINTF("PACKET SEEN\n");
        memcpy(uip_buf,&slip_io->buffer[slip_io->buffer_start],slip_io->buffer_end-slip_io->buffer_start);
        uip_len = slip_io->buffer_end-slip_io->buffer_start;
        tcpip_input();
      } else {
        PRINTF("VOID PACKET SEEN\n");
      }
      break;
    default:
#if DEBUG
      printf("[TTY-IN] ");
      for (j=slip_io->buffer_start;j<slip_io->buffer_end;++j) {
        printf("%c",slip_io->buffer[j]);
        if((slip_io->buffer[j] == '\n'
            || slip_io->buffer[j] == '\r')
              && j < slip_io->buffer_end - 1) {
          printf("[TTY-IN] ");
        }
        if(j == slip_io->buffer_end - 1
            && slip_io->buffer[j] != '\n') {
          printf("\n");
        }
      }
#endif
      break;
  }
  slip_io->buffer_end=0;
  slip_io->buffer_start=0;
  slip_io->state=STATE_OK;
}

void
tty_readable_cb (struct ev_loop *loop, struct ev_io *w, int revents)
{
  slip_io_t *slip_io;
  int i,j;

  slip_io = (slip_io_t *)w;
  if (revents & EV_ERROR) {
    ev_io_stop (loop, w);
    return;
  }

  if(revents & EV_READ) {
    slip_io->read=read(w->fd,&slip_io->temp,TTY_TEMP_BUFF_SIZE);
    for(i=0;i<slip_io->read;++i) {
      if(slip_io->buffer_end >= TTY_BUFF_SIZE) {
        for (j=slip_io->buffer_start;j<slip_io->buffer_end;++j) {
          PRINTF("%c",slip_io->buffer[j]);
          if(slip_io->buffer[j] == '\n'
              && j < slip_io->buffer_end - 1) {
          PRINTF("[TTY-IN] ");
          }
        }
        slip_io->buffer_start=0;
        slip_io->buffer_end=0;
        slip_io->state=STATE_COMMENT;
      }
      switch(slip_io->state) {
        case STATE_RUBBISH:
          if (slip_io->temp[i]==SLIP_END) {
            slip_io->state=STATE_OK;
          }
          break;
        case STATE_ESC:
          switch(slip_io->temp[i]) {
            case SLIP_ESC_END:
              slip_io->buffer[slip_io->buffer_end++]=SLIP_END;
              slip_io->state=STATE_OK;
              break;
            case SLIP_ESC_ESC:
              slip_io->buffer[slip_io->buffer_end++]=SLIP_ESC;
              slip_io->state=STATE_OK;
              break;
            default:
              slip_io->state=STATE_RUBBISH;
              break;
          }
          break;
        case STATE_OK:
          switch(slip_io->temp[i]) {
            case SLIP_END:
              handle_input(slip_io);
              break;
            case SLIP_ESC:
              slip_io->state=STATE_ESC;
              break;
            default:
              slip_io->buffer[slip_io->buffer_end++]=slip_io->temp[i];
              break;
          }
          break;
        case STATE_COMMENT:
          switch(slip_io->temp[i]) {
            case SLIP_END:
              slip_io->state=STATE_OK;
              break;
            default:
              PRINTF("%c",slip_io->temp[i]);
              break;
          }
          break;
        default:
          PRINTF("UNKNOWN STATE : %u\n", slip_io->state);
          break;
      }
    }
    slip_io->read=0;
  }
}

int
init_ttyUSBX(const int X)
{
  char      portName[64];
  struct termios tty;
  int i;

  if (slipend == NULL) {
    if ((slipend = (unsigned char *)malloc(sizeof(unsigned char))) == NULL) {
      return -1;
    } else {
      *slipend=SLIP_END;
    }
    if ((slipesc = (unsigned char *)malloc(sizeof(unsigned char))) == NULL) {
      return -1;
    } else {
      *slipesc=SLIP_ESC;
    }
    if ((slipescend = (unsigned char *)malloc(sizeof(unsigned char))) == NULL) {
      return -1;
    } else {
      *slipescend=SLIP_ESC_END;
    }
    if ((slipescesc = (unsigned char *)malloc(sizeof(unsigned char))) == NULL) {
      return -1;
    } else {
      *slipescesc=SLIP_ESC_END;
    }
  }

  if ((slip_io=(slip_io_t *)malloc(sizeof(struct slip_io_t)))==NULL)
    return -1;
  memset(slip_io,0,sizeof(struct slip_io_t));

  sprintf(portName,"/dev/ttyUSB%d",X);
  slip_io->fd=open(portName, O_RDWR | O_NONBLOCK);
  if (slip_io->fd==-1)
    return -1;
  if(tcgetattr(slip_io->fd, &tty))
    return -1;
  cfmakeraw(&tty);

  /* Nonblocking read. */
  tty.c_cc[VTIME] = 0;
  tty.c_cc[VMIN] = 0;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag &= ~HUPCL;
  tty.c_cflag &= ~CLOCAL;

  cfsetispeed(&tty, TTY_SPEED);
  cfsetospeed(&tty, TTY_SPEED);

  if(tcsetattr(slip_io->fd, TCSAFLUSH, &tty))
    return -1;

  tty.c_cflag |= CLOCAL;
  if (tcsetattr(slip_io->fd, TCSAFLUSH, &tty))
    return -1;

  i = TIOCM_DTR;
  if(ioctl(slip_io->fd, TIOCMBIS, &i))
    return -1;

  slip_io->state=STATE_OK;
  slip_io->first_end=1;

  ev_io_init (&slip_io->slip_ev, tty_readable_cb, slip_io->fd, EV_READ);
  ev_io_start(event_loop,&slip_io->slip_ev);
  return 0;
}

void
tty_output(uint8_t *ptr, int size) {
  int res, pos;
  pos = 0;
  PRINTF("TTY start : %u/%u\n", pos, size);
  while(pos<size) {
    while(pos < size
        && (ptr[pos] != *slipend)
        && (ptr[pos] != *slipesc)) {
      ++ pos;
    }
    res=write(slip_io->fd,ptr,pos);
    PRINTF("TTY pause : %u/%u\n", pos, size);
    if (pos < size) {
      PRINTF("TTY esc : %u/%u : ", pos, size);
      res=write(slip_io->fd,slipesc,1);
      if (ptr[pos] == *slipend) {
        PRINTF("end\n");
        res=write(slip_io->fd,slipescend,1);
      } else {
        PRINTF("esc\n");
        res=write(slip_io->fd,slipescesc,1);
      }
      ptr += (pos +1);
      size -= (pos +1);
      pos = 0;
    }
  }
  PRINTF("TTY end : %u/%u\n", pos, size);
  res=write(slip_io->fd,slipend,1);
}
