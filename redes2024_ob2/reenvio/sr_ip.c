#include "sr_ip.h"
#include "sr_icmp.h"
#include "sr_rt.h"
#include "sr_utils.h"

void sr_forward_ip_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len) {
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

    /* Decrementar TTL y verificar */
    ip_hdr->ip_ttl--;
    if (ip_hdr->ip_ttl == 0) {
        /* TTL expirado, enviar mensaje ICMP Tiempo Excedido al remitente */
        sr_send_icmp_error_packet(icmp_type_time_exceeded, 0, sr, ip_hdr->ip_src, packet + sizeof(sr_ethernet_hdr_t));
        return;
    }

    /* Recalcular checksum del header modificado */
    ip_hdr->ip_sum = 0;
    ip_hdr->ip_sum = cksum(ip_hdr, ip_hdr->ip_hl * 4); /* Se multiplica HL por 4 para obtener bytes */

    /* Buscar ruta en la tabla de enrutamiento */
    struct sr_rt* matching_rt_entry = lpm(sr, ip_hdr->ip_dst);
    if (matching_rt_entry == NULL) {
        /* No hay entrada coincidente, enviar mensaje ICMP Destino Red Inalcanzable al remitente */
        sr_send_icmp_error_packet(icmp_type_dest_unreachable, 0, sr, ip_hdr->ip_src, packet + sizeof(sr_ethernet_hdr_t));
        return;
    }

    /* Obtener las direcciones IP y MAC del siguiente salto */
    uint32_t next_hop_ip = (matching_rt_entry->gw.s_addr != 0) ? matching_rt_entry->gw.s_addr : ip_hdr->ip_dst;
    sr_handle_arp_lookup(sr, packet, len, next_hop_ip, matching_rt_entry->interface);
} /* -- sr_forward_ip_packet -- */