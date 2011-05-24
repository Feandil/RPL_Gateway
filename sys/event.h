#ifndef MAIN_H
#define MAIN_H

#include <ev.h>

struct ev_loop *event_loop;

void event_init(void);
void event_launch(void);
#endif /* MAIN_H */

