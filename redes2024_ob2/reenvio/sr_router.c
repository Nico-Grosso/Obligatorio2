/**********************************************************************
 * file:  sr_router.c
 *
 * Descripción:
 *
 * Este archivo contiene todas las funciones que interactúan directamente
 * con la tabla de enrutamiento, así como el método de entrada principal
 * para el enrutamiento.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"
#include "sr_icmp.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Inicializa el subsistema de enrutamiento
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    assert(sr);

    /* Inicializa la caché y el hilo de limpieza de la caché */
    sr_arpcache_init(&(sr->cache));

    /* Inicializa los atributos del hilo */
    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    /* Hilo para gestionar el timeout del caché ARP */
    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

} /* -- sr_init -- */

struct sr_rt* lpm(struct sr_instance *sr, uint32_t dest_ip){

  struct sr_rt* best_match = NULL;
  struct sr_rt* row_walker = sr->routing_table;

  while (row_walker != NULL){
    if((dest_ip & row_walker->mask.s_addr) == (row_walker->dest.s_addr & row_walker->mask.s_addr)){ /* si coincide el prefijo */
      if(best_match == NULL || row_walker->mask.s_addr > best_match->mask.s_addr){ /* si aun no se inicializo o encontre un prefijo mas grande*/
        best_match = row_walker;
      }
    }
    row_walker = row_walker->next;
  }
  if (best_match){
    printf("Best match LPM: %s\n", inet_ntoa(best_match->gw));
  }
  return best_match;
} /* -- lpm -- */

/* Envía un paquete ICMP de error */
void sr_send_icmp_error_packet(uint8_t type,          /* Tipo de mensaje ICMP */ 
                              uint8_t code,           /* Código dentro del tipo ICMP */
                              struct sr_instance *sr, /* Puntero a la instancia del router */
                              uint32_t ipDst,         /* Dirección IP de destino a la que se enviará el paquete ICMP */
                              uint8_t *ipPacket)      /* Paquete IP original que causó el error */
{
  struct sr_rt* matching_rt_entry = lpm(sr, ipDst);
  struct sr_if* iface = sr_get_interface(sr, matching_rt_entry->interface);

  uint8_t* icmp_packet = generate_icmp_packet_t3(type, code, ipPacket, sr, iface);

  unsigned int icmp_len_t3 = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);

  printf("Enviando ICMP error:");
  print_hdr_eth(icmp_packet);
  print_hdr_ip(icmp_packet + sizeof(sr_ethernet_hdr_t));
  print_hdr_icmp(icmp_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  sr_send_packet(sr, icmp_packet, icmp_len_t3, iface->name);

  free(icmp_packet);          /* TODO: ¿liberamos aca?*/

} /* -- sr_send_icmp_error_packet -- */

void sr_handle_ip_packet(struct sr_instance *sr,  /* Puntero a la instancia del router */ 
        uint8_t *packet /* lent */,               /* Puntero al paquete recibido (paquete IP) */ 
        unsigned int len,                         /* Longitut total del paquete (en bytes) */ 
        uint8_t *srcAddr,                         /* Dirección MAC de origen del paquete */ 
        uint8_t *destAddr,                        /* Dirección MAC de destino del paquete */
        char *interface /* lent */,               /* Nombre de la interfaz de red (eth0, eth1) */
        sr_ethernet_hdr_t *eHdr) {                /* Puntero al encabezado Ethernet del paquete */ 

  sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)); /* cabecera IP */
  uint8_t ip_header_length = ip_hdr->ip_hl * 4; /* tamanio de la cabecera IP en bytes */
  uint32_t src_ip = ip_hdr->ip_src; /* direccion origen IP */  
  uint32_t dest_ip = ip_hdr->ip_dst; /* direccion destino IP */
  uint8_t protocol = ip_protocol((uint8_t *) ip_hdr); /* que protocolo llega en el paquete (ICMP, TCP, etc) */  

  printf("RECIBIDO\n");
  print_hdr_eth(packet);
  print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));

  /* Verificar si el paquete está destinado a una de las interfaces del router */
  struct sr_if* iface = sr_get_interface_given_ip(sr, ip_hdr->ip_dst); /* devuelve 0 si el router no es destino */ 

  if (iface == NULL) {
      /* El paquete no está destinado al router, proceder con el reenvío */

      /* Decrementar TTL */
      ip_hdr->ip_ttl--;

      if (ip_hdr->ip_ttl == 0) {
          /* TTL expirado, enviar mensaje ICMP Tiempo Excedido al remitente */
          sr_send_icmp_error_packet(icmp_type_time_exceeded, 0, sr, src_ip, packet); /* Tipo 11, Código 0*/
          return;
      }

      /* Recalcular checksum del header modificado */
      ip_hdr->ip_sum = 0;
      ip_hdr->ip_sum = cksum(ip_hdr, ip_header_length);

      /* Buscar la entrada de la tabla de enrutamiento con la coincidencia de prefijo más larga */
      /* TODO: Averiguar si es necesario usar ntohl 
        ntohl(): Esta función convierte el valor de 32 bits de formato de red (big-endian)
        a formato de host (little-endian en la mayoría de las máquinas).
      */
      /*dest_ip = ntohl(ip_hdr->ip_dst);*/ 
      struct sr_rt* matching_rt_entry = lpm(sr, dest_ip);

      if (matching_rt_entry == NULL) {
          /* No hay entrada coincidente, enviar mensaje ICMP Destino Red Inalcanzable al remitente */
          sr_send_icmp_error_packet(icmp_type_dest_unreachable, 0, sr, src_ip, packet); /*Tipo 3, Código 0*/ 
          return;
      }

      /* Obtener la dirección IP del siguiente salto */
      /*TODO: revisar esto*/ 
      uint32_t next_hop_ip = (matching_rt_entry->gw.s_addr != 0) ? matching_rt_entry->gw.s_addr : ip_hdr->ip_dst;   

      /* Obtener la interfaz de salida */
      struct sr_if* out_interface = sr_get_interface(sr, matching_rt_entry->interface);

      /* Verificar la caché ARP */
      struct sr_arpentry *arp_entry = sr_arpcache_lookup(&(sr->cache), next_hop_ip);

      if (arp_entry != NULL && arp_entry->valid) {
          /* Tenemos la dirección MAC, proceder a enviar el paquete */
          printf("Entrada ARP encontrada, reenviando paquete\n");

          /* Construir la cabecera Ethernet con las direcciones MAC adecuadas */
          sr_ethernet_hdr_t *eth_hdr = (sr_ethernet_hdr_t *)packet;

          /* Establecer la dirección de destino Ethernet a la dirección MAC de la caché ARP */
          memcpy(eth_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);

          /* Establecer la dirección de origen Ethernet a la dirección MAC de la interfaz de salida */
          memcpy(eth_hdr->ether_shost, out_interface->addr, ETHER_ADDR_LEN);

          /* Enviar el paquete */
          print_hdr_eth(packet);
          print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));

          sr_send_packet(sr, packet, len, out_interface->name);

          free(arp_entry);

      } else {
          /* No se encontró entrada ARP, es necesario poner en cola el paquete y enviar una solicitud ARP */
          printf("No se encontró entrada ARP, enviando solicitud ARP\n");
          struct sr_arpreq* req = sr_arpcache_queuereq(&(sr->cache), next_hop_ip, packet, len, matching_rt_entry->interface);
          print_hdr_eth(packet);
          print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
          handle_arpreq(sr, req);
      }      
  } else {
      /* El paquete está destinado al propio router */
      /* Manejar solicitudes ICMP echo */
      sr_icmp_hdr_t* icmp_hdr = (sr_icmp_hdr_t*)((uint8_t *)ip_hdr + (ip_hdr->ip_hl * 4)); /*accedo al header del ICMP*/ 

      if (protocol == ip_protocol_icmp && icmp_hdr->icmp_type == icmp_echo_request){ /* verificamos si el paquete es ECHO REQUEST */

        printf("Recibido ICMP echo request, respondiendo con echo reply\n");
        
        uint8_t* icmp_packet = generate_icmp_packet(icmp_echo_reply, 0, packet, sr, iface);

        print_hdr_eth(packet);
        print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
        print_hdr_icmp(icmp_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

        unsigned int icmp_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t);

        /* Enviar el paquete */
        sr_send_packet(sr, icmp_packet, icmp_len, iface->name);

        free(icmp_packet);          /*¿liberamos aca?*/
       
      } else { /*es cualquier otro paquete, enviar ICMP error*/ 
        printf("El paquete es para mí pero no es ICMP, enviando ICMP puerto inalcanzable\n");
        print_hdr_eth(packet);
        print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
				sr_send_icmp_error_packet(icmp_type_dest_unreachable, icmp_code_port_unreachable, sr, src_ip, packet);
      }
  } 

  /* 
  * COLOQUE ASÍ SU CÓDIGO
  * SUGERENCIAS: 
  * - Obtener el cabezal IP y direcciones 
  * - Verificar si el paquete es para una de mis interfaces o si hay una coincidencia en mi tabla de enrutamiento 
  * - Si no es para una de mis interfaces y no hay coincidencia en la tabla de enrutamiento, enviar ICMP net unreachable
  * - Sino, si es para mí, verificar si es un paquete ICMP echo request y responder con un echo reply 
  * - Sino, verificar TTL, ARP y reenviar si corresponde (puede necesitar una solicitud ARP y esperar la respuesta)
  * - No olvide imprimir los mensajes de depuración
  */

}

/* 
* ***** A partir de aquí no debería tener que modificar nada ****
*/

/* Envía todos los paquetes IP pendientes de una solicitud ARP */
void sr_arp_reply_send_pending_packets(struct sr_instance *sr,
                                        struct sr_arpreq *arpReq,
                                        uint8_t *dhost,
                                        uint8_t *shost,
                                        struct sr_if *iface) {

  struct sr_packet *currPacket = arpReq->packets;
  sr_ethernet_hdr_t *ethHdr;
  uint8_t *copyPacket;

  while (currPacket != NULL) {
     ethHdr = (sr_ethernet_hdr_t *) currPacket->buf;
     memcpy(ethHdr->ether_shost, dhost, sizeof(uint8_t) * ETHER_ADDR_LEN);
     memcpy(ethHdr->ether_dhost, shost, sizeof(uint8_t) * ETHER_ADDR_LEN);

     copyPacket = malloc(sizeof(uint8_t) * currPacket->len);
     memcpy(copyPacket, ethHdr, sizeof(uint8_t) * currPacket->len);

     print_hdrs(copyPacket, currPacket->len);
     sr_send_packet(sr, copyPacket, currPacket->len, iface->name);
     currPacket = currPacket->next;
  }
}

/* Gestiona la llegada de un paquete ARP*/
void sr_handle_arp_packet(struct sr_instance *sr,
        uint8_t *packet /* lent */,
        unsigned int len,
        uint8_t *srcAddr,
        uint8_t *destAddr,
        char *interface /* lent */,
        sr_ethernet_hdr_t *eHdr) {

  /* Imprimo el cabezal ARP */
  printf("*** -> It is an ARP packet. Print ARP header.\n");
  print_hdr_arp(packet + sizeof(sr_ethernet_hdr_t));

  /* Obtengo el cabezal ARP */
  sr_arp_hdr_t *arpHdr = (sr_arp_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));

  /* Obtengo las direcciones MAC */
  unsigned char senderHardAddr[ETHER_ADDR_LEN], targetHardAddr[ETHER_ADDR_LEN];
  memcpy(senderHardAddr, arpHdr->ar_sha, ETHER_ADDR_LEN);
  memcpy(targetHardAddr, arpHdr->ar_tha, ETHER_ADDR_LEN);

  /* Obtengo las direcciones IP */
  uint32_t senderIP = arpHdr->ar_sip;
  uint32_t targetIP = arpHdr->ar_tip;
  unsigned short op = ntohs(arpHdr->ar_op);

  /* Verifico si el paquete ARP es para una de mis interfaces */
  struct sr_if *myInterface = sr_get_interface_given_ip(sr, targetIP);

  if (op == arp_op_request) {  /* Si es un request ARP */
    printf("**** -> It is an ARP request.\n");

    /* Si el ARP request es para una de mis interfaces */
    if (myInterface != 0) {
      printf("***** -> ARP request is for one of my interfaces.\n");

      /* Agrego el mapeo MAC->IP del sender a mi caché ARP */
      printf("****** -> Add MAC->IP mapping of sender to my ARP cache.\n");
      sr_arpcache_insert(&(sr->cache), senderHardAddr, senderIP);

      /* Construyo un ARP reply y lo envío de vuelta */
      printf("****** -> Construct an ARP reply and send it back.\n");
      memcpy(eHdr->ether_shost, (uint8_t *) myInterface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(eHdr->ether_dhost, (uint8_t *) senderHardAddr, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(arpHdr->ar_sha, myInterface->addr, ETHER_ADDR_LEN);
      memcpy(arpHdr->ar_tha, senderHardAddr, ETHER_ADDR_LEN);
      arpHdr->ar_sip = targetIP;
      arpHdr->ar_tip = senderIP;
      arpHdr->ar_op = htons(arp_op_reply);

      /* Imprimo el cabezal del ARP reply creado */
      print_hdrs(packet, len);

      sr_send_packet(sr, packet, len, myInterface->name);
    }

    printf("******* -> ARP request processing complete.\n");

  } else if (op == arp_op_reply) {  /* Si es un reply ARP */

    printf("**** -> It is an ARP reply.\n");

    /* Agrego el mapeo MAC->IP del sender a mi caché ARP */
    printf("***** -> Add MAC->IP mapping of sender to my ARP cache.\n");
    struct sr_arpreq *arpReq = sr_arpcache_insert(&(sr->cache), senderHardAddr, senderIP);
    
    if (arpReq != NULL) { /* Si hay paquetes pendientes */

    	printf("****** -> Send outstanding packets.\n");
    	sr_arp_reply_send_pending_packets(sr, arpReq, (uint8_t *) myInterface->addr, (uint8_t *) senderHardAddr, myInterface);
    	sr_arpreq_destroy(&(sr->cache), arpReq);

    }
    printf("******* -> ARP reply processing complete.\n");
  }
}

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* Obtengo direcciones MAC origen y destino */
  sr_ethernet_hdr_t *eHdr = (sr_ethernet_hdr_t *) packet;
  uint8_t *destAddr = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint8_t *srcAddr = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(destAddr, eHdr->ether_dhost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(srcAddr, eHdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint16_t pktType = ntohs(eHdr->ether_type);

  if (is_packet_valid(packet, len)) {
    if (pktType == ethertype_arp) {
      sr_handle_arp_packet(sr, packet, len, srcAddr, destAddr, interface, eHdr);
    } else if (pktType == ethertype_ip) {
      sr_handle_ip_packet(sr, packet, len, srcAddr, destAddr, interface, eHdr);
    }
  }

}/* end sr_ForwardPacket */