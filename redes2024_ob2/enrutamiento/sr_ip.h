#ifndef SR_IP_H
#define SR_IP

#include <stdint.h>

#include "sr_protocol.h"
#include "sr_if.h"
#include "sr_router.h"
#include "sr_arp.h"

void sr_forward_ip_packet(struct sr_instance* sr, uint8_t* packet, unsigned int len);

#endif /* SR_IP_H */