#ifndef MAIN_H
#define MAIN_H

#include <ev.h>

struct ev_loop *event_loop;

int event_init(void);
void event_launch(void);
void event_stop(void);
#endif /* MAIN_H */

