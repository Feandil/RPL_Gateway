#include "sys/event.h"

void
event_init(void)
{
  event_loop = ev_default_loop (0);
}

void
event_launch(void)
{
  ev_loop(event_loop,0);
}
