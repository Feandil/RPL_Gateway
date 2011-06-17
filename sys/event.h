#ifndef EV_H
#define EV_H

#include <ev.h>

struct ev_loop *event_loop;

int event_init(void);
void event_launch(void);
void event_stop(void);
#endif /* EV_H */

