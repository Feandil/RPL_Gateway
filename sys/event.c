#include "sys/event.h"
#include <string.h>

struct ev_timer* event_killer;

void
event_end(struct ev_loop *loop, struct ev_timer *w, int revents) {
  ev_unloop(event_loop, EVUNLOOP_ALL);
}

int
event_init(void)
{
  event_loop = ev_default_loop (EVFLAG_AUTO);
  if((event_killer = (ev_timer*) malloc(sizeof(ev_timer))) == NULL) {
    return 2;
  }
  ev_init(event_killer,event_end);
  return 0;
}

void
event_launch(void)
{
  ev_loop(event_loop,0);
}

void
event_stop(void)
{
  ev_timer_set(event_killer, 0, 0);
  ev_timer_start(event_loop, event_killer);
//  ev_unloop(event_loop,EVUNLOOP_ALL);
//  ev_unloop(event_loop,EVUNLOOP_ONE);
//  ev_break(event_loop,EVBREAK_ALL);
}

