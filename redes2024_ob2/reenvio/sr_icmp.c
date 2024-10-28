#include "sr_icmp.h"

static uint16_t ip_id_counter = 0;

void create_ethernet_header(sr_ethernet_hdr_t *eth_hdr, uint8_t *src_MAC, uint8_t *dest_MAC) {
    memcpy(eth_hdr->ether_shost, src_MAC, ETHER_ADDR_LEN);
    memcpy(eth_hdr->ether_dhost, dest_MAC, ETHER_ADDR_LEN);
    eth_hdr->ether_type = htons(ethertype_ip);
}

void create_ip_header(sr_ip_hdr_t *new_ip_hdr, sr_ip_hdr_t *original_ip_hdr, uint32_t src_ip, uint32_t dest_ip, uint16_t total_len) {
    /* TODO: es necesario copiar este header? */
    memcpy(new_ip_hdr, original_ip_hdr, sizeof(sr_ip_hdr_t));

    new_ip_hdr->ip_tos = 0;
    new_ip_hdr->ip_ttl = 16;
    new_ip_hdr->ip_hl = 5;
    new_ip_hdr->ip_v = 4;
    new_ip_hdr->ip_len = htons(total_len);
    new_ip_hdr->ip_p = ip_protocol_icmp;
    new_ip_hdr->ip_src = src_ip;
    new_ip_hdr->ip_dst = dest_ip;
    new_ip_hdr->ip_id = htons(ip_id_counter++);
    new_ip_hdr->ip_off = 0;
    new_ip_hdr->ip_sum = ip_cksum(new_ip_hdr, new_ip_hdr->ip_hl * 4);
}

/* Genera paquete ICMP */
uint8_t *generate_icmp_packet(uint8_t type, uint8_t code, uint8_t* packet, struct sr_instance* sr, struct sr_if* interface){

    /* Extraer encabezado Ethernet y direcciones MAC */
    sr_ethernet_hdr_t *eth_hdr = (sr_ethernet_hdr_t*) (packet);

    /* direcciones MAC (hay que invertirlas) */    
    uint8_t* src_MAC = eth_hdr->ether_shost;
    uint8_t* dest_MAC = eth_hdr->ether_dhost;

    /* Extraer encabezado IP */
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
    uint32_t src_ip = interface->ip; /* la direccion origen ip es la ip de la interfaz por la que entró el packet */
    uint32_t dest_ip = ip_hdr->ip_src; /* la direccion destino es el ip origen del paquete*/

    /* Longitud de los datos después de ICMP */
    int total_len = ntohs(ip_hdr->ip_len);
    int data_len = total_len - (sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t));

    /* Pedir memoria para el nuevo paquete */
    uint8_t* packet_icmp = malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t) + data_len);

    /* Crear encabezado Ethernet */
    sr_ethernet_hdr_t *new_eth_hdr = (sr_ethernet_hdr_t*) (packet_icmp);
    create_ethernet_header(new_eth_hdr, src_MAC, dest_MAC);

    /* Crear encabezado IP */
    sr_ip_hdr_t* new_ip_hdr = (sr_ip_hdr_t *)(packet_icmp + sizeof(sr_ethernet_hdr_t));
    create_ip_header(new_ip_hdr, ip_hdr, src_ip, dest_ip, sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t) + data_len);

    /* Crear encabezado ICMP */
    sr_icmp_hdr_t *new_icmp_hdr = (sr_icmp_hdr_t *)(packet_icmp + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    new_icmp_hdr->icmp_code = code;
    new_icmp_hdr->icmp_type = type;
    memcpy((uint8_t *) new_icmp_hdr + sizeof(sr_icmp_hdr_t),  (uint8_t *) ip_hdr +  sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t), data_len);
    new_icmp_hdr->icmp_sum = icmp_cksum(new_icmp_hdr, sizeof(sr_icmp_hdr_t) + data_len);
    

    return packet_icmp;
}

/* Genera paquete ICMP type3 */
uint8_t *generate_icmp_packet_t3(uint8_t type,                  /* Nº de tipo de mensaje ICMP */
                                uint8_t code,                   /* Código dentro del tipo ICMP */
                                uint8_t* packet,                /* Puntero al paquete recibido */
                                struct sr_instance* sr,         /* Puntero a la instancia del router */
                                struct sr_if* interface){       /* Puntero a la interface  */

    /* Extraer encabezado Ethernet y direcciones MAC */
    sr_ethernet_hdr_t *eth_hdr = (sr_ethernet_hdr_t*) (packet);

    /* direcciones MAC (hay que invertirlas) */
    uint8_t* src_MAC = eth_hdr->ether_dhost; 
    uint8_t* dest_MAC = eth_hdr->ether_shost;

    /* Extraer encabezado IP */
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t *)(packet);
    uint32_t src_ip = interface->ip; /* la direccion origen ip es la ip de la interfaz por la que entró el packet */
    uint32_t dest_ip = ip_hdr->ip_src; /* la direccion destino es el ip origen del paquete*/

    /* Pedir memoria para el nuevo paquete */
    uint8_t* packet_icmp = malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t));

    /* Crear encabezado Ethernet */
    sr_ethernet_hdr_t *new_eth_hdr = (sr_ethernet_hdr_t*) (packet_icmp);
    create_ethernet_header(new_eth_hdr, src_MAC, dest_MAC);

    /* Crear encabezado IP */
    sr_ip_hdr_t* new_ip_hdr = (sr_ip_hdr_t *)(packet_icmp + sizeof(sr_ethernet_hdr_t));
    create_ip_header(new_ip_hdr, ip_hdr, src_ip, dest_ip, sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t));

    /* Crear encabezado ICMP Type 3 */
    sr_icmp_t3_hdr_t *new_icmp_hdr = (sr_icmp_t3_hdr_t *)(packet_icmp + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    new_icmp_hdr->icmp_code = code;
    new_icmp_hdr->icmp_type = type;
    new_icmp_hdr->unused = 0;
    new_icmp_hdr->next_mtu = 0;
    memcpy((uint8_t*) new_icmp_hdr->data, (uint8_t*) ip_hdr, (ip_hdr->ip_hl * 4) + 8); /* 8 bytes para la carga útil */
    new_icmp_hdr->icmp_sum = icmp3_cksum(new_icmp_hdr, sizeof(sr_icmp_t3_hdr_t));

    return packet_icmp;
}