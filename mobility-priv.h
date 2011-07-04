#ifndef __MOBILITY_PRIVATE_H__
#define __MOBILITY_PRIVATE_H__


int change_route(uint8_t gw, char *command);
int change_local_route(char *command);
void clean_routes(uint8_t gw);

void mob_add_nio(uip_lladdr_t *nio, uint8_t handoff, uint8_t *buff, int *t_len, uint8_t *out_status, uint8_t *out_handoff, uint8_t gw);

void mob_send_message(struct ev_loop *loop, struct ev_timer *w, int revents);
void mob_send_lbr(uip_ip6addr_t *lbr, uint8_t flag);

void mob_new_node(uip_ds6_route_t *rep);
void mob_maj_node(uip_ds6_route_t *rep);
void mob_lost_node(uip_ip6addr_t *lbr);
void mob_new_6lbr(uip_ipaddr_t *lbr);

void mob_delete_gw(uint8_t gw);

void receive_udp(uint8_t *buffer, int read, struct sockaddr_in6 *addr, socklen_t addr_len);
void mob_proceed_up(mob_bind_up *buffer, int len, struct sockaddr_in6 *addr, socklen_t addr_len);
void mob_incoming_ack(mob_bind_ack *buffer, int len, struct sockaddr_in6 *addr);
void mob_new_gw(mob_new_lbr *target);

void mob_lbr_evolve(uint8_t old_gw, gw_list *new_gw, uint8_t new_state);
void mob_lbr_evolve_state(gw_list *gw, uint8_t new_state, uint8_t old_state);
int mob_state_evole(uint8_t new_state);

void mob_list_add_end(uip_ds6_route_t *rep);
void mob_list_remove(uip_ds6_route_t *rep);
void mob_update_min_ack(uint16_t new_ack);


#endif /* __MOBILITY_PRIVATE_H__ */
