#include "sr_arp.h"
#include "sr_if.h"
#include "sr_router.h"
#include "sr_utils.h"

void sr_handle_arp_lookup(struct sr_instance *sr, uint8_t *packet, unsigned int len, uint32_t next_hop_ip, char* interface) {
    /* Verificar la caché ARP */
    struct sr_arpentry *arp_entry = sr_arpcache_lookup(&(sr->cache), next_hop_ip);

    if (arp_entry != NULL && arp_entry->valid) {
        /* Tenemos la dirección MAC, proceder a enviar el paquete */
        printf("Entrada ARP encontrada, reenviando paquete\n");

        /* Construir la cabecera Ethernet con las direcciones MAC adecuadas */
        sr_ethernet_hdr_t *eth_hdr = (sr_ethernet_hdr_t *)packet;
        
        /* Obtener la interfaz de salida */
        struct sr_if* out_interface = sr_get_interface(sr, interface);

        /* Establecer la dirección de destino Ethernet a la dirección MAC de la caché ARP */
        memcpy(eth_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);

        /* Establecer la dirección de origen Ethernet a la dirección MAC de la interfaz de salida */
        memcpy(eth_hdr->ether_shost, out_interface->addr, ETHER_ADDR_LEN);

        print_hdr_eth(packet);
        print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));

        /* Enviar el paquete */
        sr_send_packet(sr, packet, len, out_interface->name);

        free(arp_entry);
    } else {
        /* No se encontró entrada ARP, es necesario poner en cola el paquete y enviar una solicitud ARP */
        printf("No se encontró entrada ARP, enviando solicitud ARP\n");
        sr_arpcache_queuereq(&(sr->cache), next_hop_ip, packet, len, interface);
    }
} /* -- sr_handle_arp_lookup -- */