all: prog

SOURCES_C = tcpip.c  uip6.c  uip-debug.c  uip-ds6.c  uip-icmp6.c  ttyConnection.c  tun.c  udp.c  tunnel.c  uiplib.c
SOURCES_H = conf.h  tcpip.h  uip6.h  uip_arch.h  uip-debug.h  uip-ds6.h  uip-icmp6.h  uipopt.h  ttyConnection.h  tun.h  main.h  udp.h  mobility.h  tunnel.h  uiplib.h  mobility-priv.h
EXTERN_LIB = 

include rpl/Makefile
include sys/Makefile
include lib/Makefile

SOURCES_CO = $(addsuffix o,$(SOURCES_C))
SOURCES_CO_OBJ = $(addprefix obj/,$(SOURCES_CO))

.PHONY: obj_init clean all

obj_init: obj obj/lib obj/rpl obj/sys

obj/lib:
	mkdir -p obj/lib

obj:
	mkdir -p obj

obj/rpl:
	mkdir -p obj/rpl

obj/sys:
	mkdir -p obj/sys

prog: obj_init $(SOURCES_CO_OBJ) obj/mobility.co obj/main.co
	gcc -Wall -g $(EXTERN_LIB) -o $@ $(SOURCES_CO_OBJ) obj/mobility.co obj/main.co
	sudo chown root:root $@
	sudo chmod u+s $@

clean:
	rm -f obj/*.a obj/*.co obj/*.d obj/**/*.a obj/**/*.co obj/**/*.d
	rmdir obj/lib
	rmdir obj/rpl
	rmdir obj/sys
	rmdir obj
	rm -f prog

obj/%.co: %.c $(SOURCES_H)
	gcc -Wall -Wno-unused-result -g -O -I. -MMD -c $< -o $@
	
