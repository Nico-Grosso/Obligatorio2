#ifndef SR_ARP_H
#define SR_ARP

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "sr_protocol.h"
#include "sr_router.h"

void sr_handle_arp_lookup(struct sr_instance* sr, uint8_t* packet, unsigned int len, uint32_t next_hop_ip, char* interface);

#endif /* SR_ARP_H */