#ifndef SR_ICMP_H
#define SR_ICMP_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sr_protocol.h"
#include "sr_router.h"
#include "sr_utils.h"

/* Constantes ICMP Type */
#define icmp_echo_reply 0           /* Respuesta de eco (ping reply) */
#define icmp_echo_request 8         /* Solicitud de eco (ping request) */
#define icmp_type_time_exceeded 11  /* Tiempo Excedido*/
#define icmp_type_dest_unreachable 3 /* Destino inalcanzable */

/* Códigos ICMP para Destination Unreachable */
#define icmp_code_net_unreachable 0
#define icmp_code_host_unreachable 1
#define icmp_code_port_unreachable 3



/* #==========# FUNCIONES #==========# */

/* generar paquete ICMP */

uint8_t *generate_icmp_packet(uint8_t type, uint8_t code, uint8_t* packet, struct sr_instance* sr, struct sr_if* interface);
uint8_t *generate_icmp_packet_t3(uint8_t type, uint8_t code, uint8_t* packet, struct sr_instance* sr, struct sr_if* interface);

#endif