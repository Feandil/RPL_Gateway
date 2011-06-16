all: prog

SOURCES_C = tcpip.c  uip6.c  uip-debug.c  uip-ds6.c  uip-icmp6.c  ttyConnection.c  tun.c  main.c  udp.c  mob-action.c  tunnel.c
SOURCES_H = conf.h  tcpip.h  uip6.h  uip_arch.h  uip-debug.h  uip-ds6.h  uip-icmp6.h  uipopt.h  ttyConnection.h  tun.h  main.h  udp.h  mobility.h  mob-action.h  tunnel.h 
EXTERN_LIB = 

include rpl/Makefile
include sys/Makefile
include lib/Makefile

SOURCES_CO = $(addsuffix o,$(SOURCES_C))
SOURCES_CO_OBJ = $(addprefix obj/,$(SOURCES_CO))



prog: $(SOURCES_CO_OBJ)
	gcc -Wall -g $(EXTERN_LIB) -o $@ $(SOURCES_CO_OBJ)
	sudo chown root:root prog
	sudo chmod u+s prog

clean:
	rm -f obj/*.a obj/*.co obj/*.d obj/**/*.a obj/**/*.co obj/**/*.d

obj/%.co: %.c
	gcc -Wall -g -O -I. -MMD -c $< -o $@
	
