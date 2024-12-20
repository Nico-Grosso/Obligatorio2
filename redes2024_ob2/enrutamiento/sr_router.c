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

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"
#include "pwospf_protocol.h"
#include "sr_pwospf.h"
#include "sr_arp.h"
#include "sr_icmp.h"
#include "sr_ip.h"

uint8_t sr_multicast_mac[ETHER_ADDR_LEN];

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

    /* Inicializa el subsistema OSPF */
    pwospf_init(sr);

    /* Dirección MAC de multicast OSPF */
    sr_multicast_mac[0] = 0x01;
    sr_multicast_mac[1] = 0x00;
    sr_multicast_mac[2] = 0x5e;
    sr_multicast_mac[3] = 0x00;
    sr_multicast_mac[4] = 0x00;
    sr_multicast_mac[5] = 0x05;

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

/* Envía un paquete ICMP Echo Reply */
void sr_send_icmp_echo_request(struct sr_instance *sr, uint8_t *packet, struct sr_if *iface) {
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
    struct sr_rt* matching_rt_entry = lpm(sr, ip_hdr->ip_src);

    if (matching_rt_entry == NULL) {
        /* No hay entrada coincidente, enviar mensaje ICMP Destino Red Inalcanzable al remitente */
        sr_send_icmp_error_packet(icmp_type_dest_unreachable, 0, sr, ip_hdr->ip_src, packet + sizeof(sr_ethernet_hdr_t)); /*Tipo 3, Código 0*/ 
        return;
    }

    /* Obtener la dirección IP del siguiente salto */
    uint32_t next_hop_ip = (matching_rt_entry->gw.s_addr != 0) ? matching_rt_entry->gw.s_addr : ip_hdr->ip_dst;   
    uint8_t* icmp_packet = generate_icmp_packet(icmp_echo_reply, 0, packet, sr, iface);
    sr_ip_hdr_t* ip_icmp_len = (sr_ip_hdr_t*) (icmp_packet + sizeof(sr_ethernet_hdr_t));
    unsigned int icmp_len = sizeof(sr_ethernet_hdr_t) + ntohs(ip_icmp_len->ip_len);

    sr_handle_arp_lookup(sr, icmp_packet, icmp_len, next_hop_ip, matching_rt_entry->interface);
    /*print_hdr_icmp(icmp_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));*/
    free(icmp_packet);
} /* -- sr_send_icmp_echo_request -- */

/* Envía un paquete ICMP de error */
void sr_send_icmp_error_packet(uint8_t type,          /* Tipo de mensaje ICMP */ 
                              uint8_t code,           /* Código dentro del tipo ICMP */
                              struct sr_instance *sr, /* Puntero a la instancia del router */
                              uint32_t ipDst,         /* Dirección IP de destino a la que se enviará el paquete ICMP */
                              uint8_t *ipPacket)      /* Paquete IP original que causó el error */
{
  struct sr_rt* matching_rt_entry = lpm(sr, ipDst);

  struct sr_if* out_interface = sr_get_interface(sr, matching_rt_entry->interface);

  uint8_t* icmp_packet = generate_icmp_packet_t3(type, code, ipPacket, sr, out_interface);
  unsigned int icmp_len_t3 = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
  uint32_t next_hop_ip = (matching_rt_entry->gw.s_addr != 0) ? matching_rt_entry->gw.s_addr : ipDst;   

  sr_handle_arp_lookup(sr, icmp_packet, icmp_len_t3, next_hop_ip, matching_rt_entry->interface);
  /*print_hdr_icmp(icmp_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));*/
  free(icmp_packet);
} /* -- sr_send_icmp_error_packet -- */

void sr_handle_ip_packet(struct sr_instance *sr,  /* Puntero a la instancia del router */ 
        uint8_t *packet /* lent */,               /* Puntero al paquete recibido (paquete IP) */ 
        unsigned int len,                         /* Longitud total del paquete (en bytes) */ 
        uint8_t *srcAddr,                         /* Dirección MAC de origen del paquete */ 
        uint8_t *destAddr,                        /* Dirección MAC de destino del paquete */
        char *interface /* lent */,               /* Nombre de la interfaz de red (eth0, eth1) */
        sr_ethernet_hdr_t *eHdr)                  /* Puntero al encabezado Ethernet del paquete */ 
{                
  sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)); /* cabecera IP */

  /*
  printf("RECIBIDO\n");
  print_hdr_eth(packet);
  print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
  */

  /* Verificar si el paquete es PWOSPF */
  uint8_t protocol = ip_protocol((uint8_t *) ip_hdr); /* que protocolo llega en el paquete (ICMP, TCP, OSPF, etc) */  
  if (protocol == ip_protocol_ospfv2) {
    /* Llamar al manejador de paquetes PWOSPF */
    struct sr_if* recv_iface = sr_get_interface(sr, interface);
    print_hdr_ospf(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    sr_handle_pwospf_packet(sr, packet, len, recv_iface);
    return;  /* Termina aquí si es un paquete PWOSPF */
  }

  /* Verificar si el paquete está destinado a una de las interfaces del router */
  struct sr_if* dest_iface = sr_get_interface_given_ip(sr, ip_hdr->ip_dst);

  if (dest_iface == NULL) {
      /* El paquete no está destinado al router, proceder con el reenvío */      
      sr_forward_ip_packet(sr, packet, len);
  } else {
      /* El paquete está destinado al propio router */
      sr_icmp_hdr_t* icmp_hdr = (sr_icmp_hdr_t*)((uint8_t *)ip_hdr + (ip_hdr->ip_hl * 4)); /* accedo al header del ICMP */ 

      if (protocol == ip_protocol_icmp && icmp_hdr->icmp_type == icmp_echo_request) { /* verificamos si el paquete es ECHO REQUEST */  
        /*
        printf("Print de hdr_icmp del paquete de entrada\n");
        print_hdr_icmp(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
        printf("Recibido ICMP echo request, respondiendo con echo reply\n");
        */

        sr_send_icmp_echo_request(sr, packet, dest_iface);       
      } else { /*es cualquier otro paquete, enviar ICMP error*/         
        printf("El paquete es para mí pero no es ICMP, enviando ICMP puerto inalcanzable\n");

				sr_send_icmp_error_packet(icmp_type_dest_unreachable, icmp_code_port_unreachable, sr, ip_hdr->ip_src, packet + sizeof(sr_ethernet_hdr_t));
      }
  } 

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