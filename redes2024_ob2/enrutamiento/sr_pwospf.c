/*-----------------------------------------------------------------------------
 * file: sr_pwospf.c
 *
 * Descripción:
 * Este archivo contiene las funciones necesarias para el manejo de los paquetes
 * OSPF.
 *
 *---------------------------------------------------------------------------*/

#include "sr_pwospf.h"
#include "sr_router.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>

#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "sr_utils.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"
#include "sr_rt.h"
#include "pwospf_neighbors.h"
#include "pwospf_topology.h"
#include "dijkstra.h"
#include "sr_arp.h"

/*pthread_t hello_thread;*/
pthread_t g_hello_packet_thread;
pthread_t g_all_lsu_thread;
pthread_t g_lsu_thread;
pthread_t g_neighbors_thread;
pthread_t g_topology_entries_thread;
pthread_t g_rx_lsu_thread;
pthread_t g_dijkstra_thread;

pthread_mutex_t g_dijkstra_mutex = PTHREAD_MUTEX_INITIALIZER;

struct in_addr g_router_id;
uint8_t g_ospf_multicast_mac[ETHER_ADDR_LEN];
struct ospfv2_neighbor* g_neighbors;
struct pwospf_topology_entry* g_topology;
uint16_t g_sequence_num;

/* -- Declaración de hilo principal de la función del subsistema pwospf --- */
static void* pwospf_run_thread(void* arg);

static uint16_t ip_id_counter = 0;

/*---------------------------------------------------------------------
 * Method: pwospf_init(..)
 *
 * Configura las estructuras de datos internas para el subsistema pwospf
 * y crea un nuevo hilo para el subsistema pwospf.
 *
 * Se puede asumir que las interfaces han sido creadas e inicializadas
 * en este punto.
 *---------------------------------------------------------------------*/

int pwospf_init(struct sr_instance* sr)
{
    assert(sr);

    sr->ospf_subsys = (struct pwospf_subsys*)malloc(sizeof(struct
                                                      pwospf_subsys));

    assert(sr->ospf_subsys);
    pthread_mutex_init(&(sr->ospf_subsys->lock), 0);

    g_router_id.s_addr = 0;

    /* Defino la MAC de multicast a usar para los paquetes HELLO */
    g_ospf_multicast_mac[0] = 0x01;
    g_ospf_multicast_mac[1] = 0x00;
    g_ospf_multicast_mac[2] = 0x5e;
    g_ospf_multicast_mac[3] = 0x00;
    g_ospf_multicast_mac[4] = 0x00;
    g_ospf_multicast_mac[5] = 0x05;

    g_neighbors = NULL;

    g_sequence_num = 0;


    struct in_addr zero;
    zero.s_addr = 0;
    g_neighbors = create_ospfv2_neighbor(zero);
    g_topology = create_ospfv2_topology_entry(zero, zero, zero, zero, zero, 0);

    /* -- start thread subsystem -- */
    if( pthread_create(&sr->ospf_subsys->thread, 0, pwospf_run_thread, sr)) { 
        perror("pthread_create");
        assert(0);
    }

    return 0; /* success */
} /* -- pwospf_init -- */


/*---------------------------------------------------------------------
 * Method: pwospf_lock
 *
 * Lock mutex associated with pwospf_subsys
 *
 *---------------------------------------------------------------------*/

void pwospf_lock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_lock(&subsys->lock) )
    { assert(0); }
}

/*---------------------------------------------------------------------
 * Method: pwospf_unlock
 *
 * Unlock mutex associated with pwospf subsystem
 *
 *---------------------------------------------------------------------*/

void pwospf_unlock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_unlock(&subsys->lock) )
    { assert(0); }
} 

/*---------------------------------------------------------------------
 * Method: pwospf_run_thread
 *
 * Hilo principal del subsistema pwospf.
 *
 *---------------------------------------------------------------------*/

static
void* pwospf_run_thread(void* arg)
{
    sleep(5);

    struct sr_instance* sr = (struct sr_instance*)arg;

    /* Set the ID of the router */
    while(g_router_id.s_addr == 0)
    {
        struct sr_if* int_temp = sr->if_list;
        while(int_temp != NULL)
        {
            if (int_temp->ip > g_router_id.s_addr)
            {
                g_router_id.s_addr = int_temp->ip;
            }

            int_temp = int_temp->next;
        }
    }
    Debug("\n\nPWOSPF: Selecting the highest IP address on a router as the router ID\n");
    Debug("-> PWOSPF: The router ID is [%s]\n", inet_ntoa(g_router_id));


    Debug("\nPWOSPF: Detecting the router interfaces and adding their networks to the routing table\n");
    struct sr_if* int_temp = sr->if_list;
    while(int_temp != NULL)
    {
        struct in_addr ip;
        ip.s_addr = int_temp->ip;
        struct in_addr gw;
        gw.s_addr = 0x00000000;
        struct in_addr mask;
        mask.s_addr =  int_temp->mask;
        struct in_addr network;
        network.s_addr = ip.s_addr & mask.s_addr;

        if (check_route(sr, network) == 0)
        {
            Debug("-> PWOSPF: Adding the directly connected network [%s, ", inet_ntoa(network));
            Debug("%s] to the routing table\n", inet_ntoa(mask));
            sr_add_rt_entry(sr, network, gw, mask, int_temp->name, 1);
        }
        int_temp = int_temp->next;
    }
    
    Debug("\n-> PWOSPF: Printing the forwarding table\n");
    sr_print_routing_table(sr);


    pthread_create(&g_hello_packet_thread, NULL, send_hellos, sr);
    pthread_create(&g_all_lsu_thread, NULL, send_all_lsu, sr);
    pthread_create(&g_neighbors_thread, NULL, check_neighbors_life, NULL);
    pthread_create(&g_topology_entries_thread, NULL, check_topology_entries_age, sr);

    return NULL;
} /* -- run_ospf_thread -- */

/***********************************************************************************
 * Métodos para el manejo de los paquetes HELLO y LSU
 * SU CÓDIGO DEBERÍA IR AQUÍ
 * *********************************************************************************/

/*---------------------------------------------------------------------
 * Method: check_neighbors_life
 *
 * Chequea si los vecinos están vivos
 *
 *---------------------------------------------------------------------*/

void* check_neighbors_life(void* arg)
{
    /* 
    Cada 1 segundo, chequea la lista de vecinos.
    */

    while (1){
        usleep(1000000);
        check_neighbors_alive(g_neighbors);
    }
    return NULL;
} /* -- check_neighbors_life -- */


/*---------------------------------------------------------------------
 * Method: check_topology_entries_age
 *
 * Check if the topology entries are alive 
 * and if they are not, remove them from the topology table
 *
 *---------------------------------------------------------------------*/

void* check_topology_entries_age(void* arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;

    while (1) {
        usleep(1000000);

        uint8_t deleted = check_topology_age(g_topology);

        if (deleted){ /* hubo cambios en la topologia, se elimino al menos un router */

            Debug("Imprimiendo topology table:\n");
            print_topolgy_table(g_topology);
            Debug("\n");      

            dijkstra_param_t* dij = malloc(sizeof(dijkstra_param_t));
            dij->sr = sr;
            dij->topology = g_topology;
            dij->rid = g_router_id;
            
            pthread_create(&g_dijkstra_thread, NULL, run_dijkstra, dij);

            /* TODO: revisar si esto es necesario */
            /* free(dij); */
        }
    }

    /* 
    Cada 1 segundo, chequea el tiempo de vida de cada entrada
    de la topologia.
    Si hay un cambio en la topología, se llama a la función de Dijkstra
    en un nuevo hilo.
    Se sugiere también imprimir la topología resultado del chequeo.
    */

    return NULL;
} /* -- check_topology_entries_age -- */


/*---------------------------------------------------------------------
 * Method: send_hellos
 *
 * Para cada interfaz y cada helloint segundos, construye mensaje 
 * HELLO y crea un hilo con la función para enviar el mensaje.
 *
 *---------------------------------------------------------------------*/

void* send_hellos(void* arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;

    /* While true */
    while(1)
    {
        /* Se ejecuta cada 1 segundo */
        usleep(1000000);

        /* Bloqueo para evitar mezclar el envío de HELLOs y LSUs */
        pwospf_lock(sr->ospf_subsys);

        /* Chequeo todas las interfaces para enviar el paquete HELLO */
            /* Cada interfaz matiene un contador en segundos para los HELLO*/
            /* Reiniciar el contador de segundos para HELLO */
        struct sr_if* iface = sr->if_list;
        while (iface) {
            if (iface->is_active == 0) {
                iface = iface->next;
                continue;
            }

            if (iface->helloint == 0){
                /* Estructura de parámetros para el hilo */
                struct powspf_hello_lsu_param* hello_param = (struct powspf_hello_lsu_param*)malloc(sizeof(struct powspf_hello_lsu_param));
                hello_param->sr = sr;
                hello_param->interface = iface;

                /* Nuevo hilo para enviar el paquete HELLO */ 
                pthread_create(&g_hello_packet_thread, NULL, send_hello_packet, hello_param);

                /* Reinicia el contador */
                iface->helloint = OSPF_DEFAULT_HELLOINT;
            }
            else {
                iface->helloint -= 1;
            }
            iface = iface->next;
        }

        /* Desbloqueo */
        pwospf_unlock(sr->ospf_subsys);
    };

    return NULL;
} /* -- send_hellos -- */


/*---------------------------------------------------------------------
 * Method: send_hello_packet
 *
 * Recibe un mensaje HELLO, agrega cabezales y lo envía por la interfaz
 * correspondiente.
 *
 *---------------------------------------------------------------------*/

void* send_hello_packet(void* arg)
{
    powspf_hello_lsu_param_t* hello_param = ((powspf_hello_lsu_param_t*)(arg));
    struct sr_if* iface = hello_param->interface;

    Debug("\n\nPWOSPF: Constructing HELLO packet for interface %s\n", iface->name);

    /* Tamaño del paquete HELLO */
    unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t);
    uint8_t* packet = malloc(packet_len);

    /* Encabezado Ethernet */
    sr_ethernet_hdr_t* eth_hdr = (sr_ethernet_hdr_t*) packet;
    
    /* Seteo la dirección MAC de multicast para la trama a enviar */
    memcpy(eth_hdr->ether_dhost, g_ospf_multicast_mac, ETHER_ADDR_LEN);
    
    /* Seteo la dirección MAC origen con la dirección de mi interfaz de salida */
    memcpy(eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
    
    /* Seteo el ether_type en el cabezal Ethernet */
    eth_hdr->ether_type = htons(ethertype_ip);                           

    /* Encabezado IP */
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
    
    /* Inicializo cabezal IP */
    ip_hdr->ip_v = 4;                                   
    ip_hdr->ip_hl = sizeof(sr_ip_hdr_t) / 4;            
    ip_hdr->ip_tos = 0;                                 
    ip_hdr->ip_len = htons(packet_len - sizeof(sr_ethernet_hdr_t));
    ip_hdr->ip_id = htons(ip_id_counter++);                           
    ip_hdr->ip_off = 0;                                 
    ip_hdr->ip_ttl = 16;                                
    
    /* Seteo IP origen con la IP de mi interfaz de salida */
    ip_hdr->ip_src = iface->ip;                         
    
    /* Seteo IP destino con la IP de Multicast dada: OSPF_AllSPFRouters  */
    ip_hdr->ip_dst = htonl(OSPF_AllSPFRouters);         

    /* Seteo el protocolo en el cabezal IP para ser el de OSPF (89) */
    ip_hdr->ip_p = ip_protocol_ospfv2;      

    /* Calculo y seteo el chechsum IP*/            
    ip_hdr->ip_sum = 0;
    ip_hdr->ip_sum = cksum(ip_hdr, ip_hdr->ip_hl * 4); 

    /* Encabezado PWOSPF */
    ospfv2_hdr_t* ospf_hdr = (ospfv2_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

    /* Inicializo cabezal de PWOSPF con version 2 y tipo HELLO */
    ospf_hdr->version = OSPF_V2;
    ospf_hdr->type = OSPF_TYPE_HELLO;                   
    ospf_hdr->len = htons(sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t));
    
    /* Seteo el Router ID con mi ID*/
    ospf_hdr->rid = g_router_id.s_addr;                 
     
    /* Seteo el Area ID en 0 */
    ospf_hdr->aid = 0;                           
    
    /* Seteo el Authentication Type y Authentication Data en 0*/
    ospf_hdr->autype = 0;
    ospf_hdr->audata = 0;

    /* Encabezado HELLO */
    ospfv2_hello_hdr_t* hello_hdr = (ospfv2_hello_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
    
    /* Seteo máscara con la máscara de mi interfaz de salida */
    hello_hdr->nmask = iface->mask;
    
    /* Seteo Hello Interval con OSPF_DEFAULT_HELLOINT */
    hello_hdr->helloint = OSPF_DEFAULT_HELLOINT;
    
    /* Seteo Padding en 0*/
    hello_hdr->padding = 0;

    /* Calculo y actualizo el checksum del cabezal OSPF */
    ospf_hdr->csum = 0;
    ospf_hdr->csum = cksum(ospf_hdr, sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t));

    /* Envío el paquete HELLO */
    sr_send_packet(hello_param->sr, packet, packet_len, iface->name);
    
    /* Imprimo información del paquete HELLO enviado */
    Debug("-> PWOSPF: Sending HELLO Packet of length = %d, out of the interface: %s\n", packet_len, hello_param->interface->name);
    Debug("      [Router ID = %s]\n", inet_ntoa(g_router_id));
    /* TODO: ip y mask
    Debug("      [Router IP = %s]\n", inet_ntoa(ip));
    Debug("      [Network Mask = %s]\n", inet_ntoa(mask));
    */
    Debug("-> PWOSPF: HELLO Packet sent on interface: %s\n", iface->name);

    free(packet);
    free(hello_param);

    return NULL;
}
 /* -- send_hello_packet -- */

/*---------------------------------------------------------------------
 * Method: send_all_lsu
 *
 * Construye y envía LSUs cada 30 segundos
 *
 *---------------------------------------------------------------------*/

void* send_all_lsu(void* arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;

    while (1) {
        /* Se ejecuta cada OSPF_DEFAULT_LSUINT segundos */
        usleep(OSPF_DEFAULT_LSUINT * 1000000);

        /* Bloqueo para evitar mezclar el envío de HELLOs y LSUs */
        pwospf_lock(sr->ospf_subsys);

        /* Recorro todas las interfaces para enviar el paquete LSU */
        struct sr_if* iface = sr->if_list;
        while (iface) {
            if (iface->is_active == 0) {
                iface = iface->next;
                continue;
            }
            /* Recorro la lista de vecinos para ver si hay un vecino activo en esta interfaz */
            struct ospfv2_neighbor* neighbor = g_neighbors;
            while (neighbor) {
                /* Enviar LSU si hay un vecino activo */
                if (neighbor->alive) {
                    powspf_hello_lsu_param_t* lsu_param = malloc(sizeof(powspf_hello_lsu_param_t));
                    lsu_param->sr = sr;
                    lsu_param->interface = iface;

                    /* Crear un hilo para enviar el LSU en esta interfaz */
                    pthread_t lsu_thread;
                    pthread_create(&lsu_thread, NULL, send_lsu, lsu_param); /* Enviar lsu de forma asincrona */
                    pthread_detach(lsu_thread); /* Detach para evitar fugas de memoria */
                    break; /*Salir del bucle de vecinos, ya que se encontró uno activo */
                }                
                neighbor = neighbor->next;
            }
            iface = iface->next;
        }

        /* Desbloqueo */
        pwospf_unlock(sr->ospf_subsys);
    }
    return NULL;
}
 /* -- send_all_lsu -- */

/*---------------------------------------------------------------------
 * Method: send_lsu
 *
 * Construye y envía paquetes LSU a través de una interfaz específica
 *
 *---------------------------------------------------------------------*/

void* send_lsu(void* arg)
{
    powspf_hello_lsu_param_t* lsu_param = ((powspf_hello_lsu_param_t*)(arg));

    /* Solo envío LSUs si del otro lado hay un router*/
    /*--  Esto lo chequeo antes de llamar a la funcion pero igual --*/
    if (lsu_param->interface->neighbor_id == NEIGHBOR_ID_UNITIALIZED){
        return NULL;    
    }

    /* Construyo el LSU */
    Debug("\n\nPWOSPF: Constructing LSU packet\n");

    int cantidad_rutas = count_routes(lsu_param->sr);

    uint8_t* lsu_packet = malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + sizeof(ospfv2_lsa_t)*cantidad_rutas);
    int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + sizeof(ospfv2_lsa_t)*cantidad_rutas;
    int ospf_len = sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + sizeof(ospfv2_lsa_t)*cantidad_rutas;

    /* Inicializo cabezal Ethernet */
    /* Dirección MAC destino la dejo para el final ya que hay que hacer ARP */
    sr_ethernet_hdr_t* eth_hdr = (sr_ethernet_hdr_t*) lsu_packet;
    memcpy(eth_hdr->ether_shost, (uint8_t*) lsu_param->interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
    eth_hdr->ether_type = htons(ethertype_ip);

    /* Inicializo cabezal IP*/
    /* La IP destino es la del vecino contectado a mi interfaz*/
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t*)((uint8_t*)eth_hdr + sizeof(sr_ethernet_hdr_t));
    ip_hdr->ip_tos = 0;
    ip_hdr->ip_ttl = 16;
    ip_hdr->ip_hl = 5;
    ip_hdr->ip_v = 4;
    ip_hdr->ip_off = 0;
    ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + sizeof(ospfv2_lsa_t)*cantidad_rutas);
    ip_hdr->ip_p = ip_protocol_ospfv2;
    ip_hdr->ip_src = lsu_param->interface->ip;
    ip_hdr->ip_dst = lsu_param->interface->neighbor_ip;
    ip_hdr->ip_id = htons(ip_id_counter++);
    ip_hdr->ip_sum = ip_cksum(ip_hdr, ip_hdr->ip_hl*4);
   
    /* Inicializo cabezal de OSPF*/
    ospfv2_hdr_t* ospf_hdr = (ospfv2_hdr_t*)((uint8_t*)ip_hdr + sizeof(sr_ip_hdr_t));
    ospf_hdr->version = OSPF_V2;
    ospf_hdr->type = OSPF_TYPE_LSU;
    ospf_hdr->len = htons(ospf_len);
    ospf_hdr->rid = g_router_id.s_addr;
    ospf_hdr->aid = 0;
    ospf_hdr->csum = 0; /*-- lo inicializo despues --*/
    ospf_hdr->autype = 0;
    ospf_hdr->audata = 0;

    /* Seteo el número de secuencia y avanzo*/
    g_sequence_num++;
    /* Seteo el TTL en 64 y el resto de los campos del cabezal de LSU */
    ospfv2_lsu_hdr_t* lsu_hdr = (ospfv2_lsu_hdr_t*)((uint8_t*)ospf_hdr + sizeof(ospfv2_hdr_t));
    lsu_hdr->seq = htons(g_sequence_num);
    lsu_hdr->unused = 0;
    lsu_hdr->ttl = 64;
    /* Seteo el número de anuncios con la cantidad de rutas a enviar. Uso función count_routes */
    lsu_hdr->num_adv = htonl(cantidad_rutas);

    /* Creo el paquete y seteo todos los cabezales del paquete a transmitir */
    /*-- Ya lo creé arriba y ya seteé todo -- */

    /* Creo cada LSA iterando en las entradas de la tabla */
    struct sr_rt* line = lsu_param->sr->routing_table;
    int i = 0;
    while (line != NULL){
        /* Solo envío entradas directamente conectadas y agregadas a mano*/
        /* Creo LSA con subnet, mask y routerID (id del vecino de la interfaz)*/
        if (line->admin_dst <= 1){ /*-- 0 o 1 son directamente conectadas o estáticas --*/
            ospfv2_lsa_t* lsa_hdr = (ospfv2_lsa_t*)((uint8_t*)lsu_hdr + sizeof(ospfv2_lsu_hdr_t) + sizeof(ospfv2_lsa_t)*i);   
            lsa_hdr->subnet = line->dest.s_addr;
            lsa_hdr->mask = line->mask.s_addr;
            lsa_hdr->rid = sr_get_interface(lsu_param->sr, line->interface)->neighbor_id;
            i++;
        }
        line = line->next;
    }

    /* Calculo el checksum del paquete LSU */
    ospf_hdr->csum = ospfv2_cksum(ospf_hdr, ospf_len);

    /* Me falta la MAC para poder enviar el paquete, la busco en la cache ARP*/
    /* Envío el paquete si obtuve la MAC o lo guardo en la cola para cuando tenga la MAC*/
    sr_handle_arp_lookup(lsu_param->sr, lsu_packet, packet_len, lsu_param->interface->neighbor_ip, lsu_param->interface->name);
    
   /* Libero memoria */
   free(lsu_packet);
   free(lsu_param);

    return NULL;
} /* -- send_lsu -- */


/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_hello_packet
 *
 * Gestiona los paquetes HELLO recibidos
 *
 *---------------------------------------------------------------------*/

void sr_handle_pwospf_hello_packet(struct sr_instance* sr, uint8_t* packet, unsigned int length, struct sr_if* rx_if)
{
    
    /* Obtengo información del paquete recibido */
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t));
    ospfv2_hdr_t* ospfv2_hdr = ((ospfv2_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)));
    ospfv2_hello_hdr_t* ospfv2_hello_hdr =(ospfv2_hello_hdr_t*) ((uint8_t*)ospfv2_hdr + sizeof(ospfv2_hdr_t));

    sr_print_routing_table(sr);

    struct in_addr neighbor_id;
    neighbor_id.s_addr = ospfv2_hdr->rid;

    struct in_addr neighbor_ip;
    neighbor_ip.s_addr = ip_hdr->ip_src;

    struct in_addr net_mask;
    net_mask.s_addr = ospfv2_hello_hdr->nmask;  

 
    /* Imprimo info del paquete recibido*/
  
    Debug("-> PWOSPF: Detecting PWOSPF HELLO Packet from:\n");
    Debug("      [Neighbor ID = %s]\n", inet_ntoa(neighbor_id));
    Debug("      [Neighbor IP = %s]\n", inet_ntoa(neighbor_ip));
    Debug("      [Network Mask = %s]\n", inet_ntoa(net_mask));

    /* Chequeo checksum */

    uint16_t sum = ospfv2_hdr->csum;
    ospfv2_hdr->csum = 0;
    uint16_t calcular_chk = ospfv2_cksum(ospfv2_hdr, sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t));
    if (sum != calcular_chk){
        Debug("-> PWOSPF: HELLO Packet dropped, invalid checksum\n");
        return;
    }
    ospfv2_hdr->csum = calcular_chk;

    /* Chequeo de la máscara de red */
    if (rx_if->mask != net_mask.s_addr){
        Debug("-> PWOSPF: HELLO Packet dropped, invalid hello network mask\n");
        return;
    }

    /* Chequeo del intervalo de HELLO */
    if (ospfv2_hello_hdr->helloint != OSPF_DEFAULT_HELLOINT){
        Debug("-> PWOSPF: HELLO Packet dropped, invalid hello interval\n");
        return;
    }

    /* Seteo el vecino en la interfaz por donde llegó y actualizo la lista de vecinos */
    int es_nuevo = 0;
    if (rx_if->neighbor_id != ospfv2_hdr->rid){ /* si el neighbor_id = 0 es porque no estaba guaradado ese vecino*/
        es_nuevo = 1;
        rx_if->neighbor_id = ospfv2_hdr->rid;
        rx_if->neighbor_ip = neighbor_ip.s_addr;
    }

    refresh_neighbors_alive(g_neighbors, neighbor_id);

    /* Si es un nuevo vecino, debo enviar LSUs por todas mis interfaces*/
        /* Recorro todas las interfaces para enviar el paquete LSU */
        /* Si la interfaz tiene un vecino, envío un LSU */
    if (es_nuevo){
        struct sr_if* interface = sr->if_list;
        while (interface != NULL){
            if (interface->neighbor_id != NEIGHBOR_ID_UNITIALIZED && interface->neighbor_id !=  NEIGHBOR_ID_BROADCAST){ /*-- Si tiene estas ID es porque no es un vecino inicializado-- */
                powspf_hello_lsu_param_t* lsu_param = (powspf_hello_lsu_param_t*)(malloc(sizeof(powspf_hello_lsu_param_t)));
                lsu_param->interface = interface;
                lsu_param->sr = sr;
                pthread_create(&g_lsu_thread, NULL, send_lsu, lsu_param);
            }
            interface = interface->next;
        }
    }

} /* -- sr_handle_pwospf_hello_packet -- */


/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_lsu_packet
 *
 * Gestiona los paquetes LSU recibidos y actualiza la tabla de topología
 * y ejecuta el algoritmo de Dijkstra
 *
 *---------------------------------------------------------------------*/

void* sr_handle_pwospf_lsu_packet(void* arg)
{
    powspf_rx_lsu_param_t* rx_lsu_param = ((powspf_rx_lsu_param_t*)(arg));
    
    /* Extraigo los componentes de la estructura */
    struct sr_instance* sr = rx_lsu_param->sr;
    uint8_t* packet = rx_lsu_param->packet;
    struct sr_if* rx_if = rx_lsu_param->rx_if;
    unsigned int length = rx_lsu_param->length;
    
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t));
    struct ospfv2_hdr* ospf_hdr = (struct ospfv2_hdr*)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

    struct in_addr addr_id, addr_ip;
    addr_id.s_addr = ospf_hdr->rid;
    addr_ip.s_addr = ip_hdr->ip_src;
    /* Imprimo info del paquete recibido*/
    Debug("-> PWOSPF: Detecting LSU Packet from [Neighbor ID = %s, IP = %s]\n", inet_ntoa(addr_id), inet_ntoa(addr_ip));
    /* Chequeo checksum */
    uint16_t ospf_len = ntohs(ospf_hdr->len);
    uint16_t calculated_checksum = ospfv2_cksum(ospf_hdr, ospf_len);

    if (calculated_checksum != ospf_hdr->csum){
        Debug("-> PWOSPF: LSU Packet dropped, invalid checksum\n");
        return NULL;
    }

    /* Obtengo el Router ID del router originario del LSU y chequeo si no es mío*/
    struct in_addr origin_router_id;
    origin_router_id.s_addr = ospf_hdr->rid;

    if (origin_router_id.s_addr == g_router_id.s_addr){
        Debug("-> PWOSPF: LSU Packet dropped, originated by this router\n");
        return NULL;
    }

    /* Obtengo el número de secuencia y uso check_sequence_number para ver si ya lo recibí desde ese vecino*/
    ospfv2_lsu_hdr_t* lsu_hdr = (ospfv2_lsu_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
    uint16_t sequence_num = ntohs(lsu_hdr->seq);

    if(check_sequence_number(g_topology, origin_router_id, sequence_num) == 0){
        Debug("-> PWOSPF: LSU Packet dropped, repeated sequence number\n");
        return NULL;
    }

    /* Itero en los LSA que forman parte del LSU. Para cada uno, actualizo la topología.*/
    Debug("-> PWOSPF: Processing LSAs and updating topology table\n");
    /* Obtengo subnet */
    /* Obtengo vecino */

    /* Puntero inicial para el primer LSA después de las cabeceras */
    uint8_t* lsa_ptr = packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t);

    struct in_addr next_hop;
    next_hop.s_addr = ip_hdr->ip_src;

    unsigned int i;
    for (i = 0; i < ntohl(lsu_hdr->num_adv); i++) {
        ospfv2_lsa_t* lsa = (ospfv2_lsa_t*)(lsa_ptr + (i * sizeof(ospfv2_lsa_t)));
        
        struct in_addr net_num, net_mask, neighbor_id;
         
        net_num.s_addr = lsa->subnet;   /* La dirección de la subred */ 
        net_mask.s_addr = lsa->mask;    /* La máscara de red */ 
        neighbor_id.s_addr = lsa->rid;  

        /* Imprimo info de la entrada de la topología */
        Debug("      [Subnet = %s]", inet_ntoa(net_num));
        Debug("      [Mask = %s]", inet_ntoa(net_mask));
        Debug("      [Neighbor ID = %s]\n", inet_ntoa(neighbor_id));

        /* LLamo a refresh_topology_entry*/
        refresh_topology_entry(g_topology, origin_router_id, net_num, net_mask, neighbor_id, next_hop, sequence_num);
    }       
      
    /* Imprimo la topología */
    Debug("\n-> PWOSPF: Printing the topology table\n");
    print_topolgy_table(g_topology);
    
    /* Ejecuto Dijkstra en un nuevo hilo (run_dijkstra)*/
    dijkstra_param_t* dij_param = (dijkstra_param_t*)(malloc(sizeof(dijkstra_param_t)));
    dij_param->sr = sr;
    dij_param->topology = g_topology;
    dij_param->mutex = g_dijkstra_mutex;
    pthread_create(&g_dijkstra_thread, NULL, run_dijkstra, dij_param);

    /* Flooding del LSU por todas las interfaces menos por donde me llegó */
    struct sr_if* iface = sr->if_list;
    while (iface) {
        if (strcmp(iface->name, rx_if->name) != 0 && iface->neighbor_id != NEIGHBOR_ID_UNITIALIZED) {
            
            /* Seteo MAC de origen */
            int i;
            for (i = 0; i < ETHER_ADDR_LEN; i++) {
                ((sr_ethernet_hdr_t*)(packet))->ether_shost[i] = iface->addr[i];
            }

            /* Ajusto paquete IP, origen y checksum*/
            ip_hdr->ip_src = iface->ip;
            ip_hdr->ip_sum = ip_cksum(ip_hdr, sizeof(sr_ip_hdr_t));

            /* Ajusto cabezal OSPF: checksum y TTL*/
            lsu_hdr->ttl--;

            /* checksum OSPF */
            ospf_hdr->csum = ospfv2_cksum(ospf_hdr, htons(ospf_hdr->len));
            
            /* Envío el paquete*/
            sr_send_packet(sr, packet, length, iface->name);
        }
        iface = iface->next;
    }

    free(rx_lsu_param);     
    
    return NULL;
} /* -- sr_handle_pwospf_lsu_packet -- */

/**********************************************************************************
 * SU CÓDIGO DEBERÍA TERMINAR AQUÍ
 * *********************************************************************************/

/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_packet
 *
 * Gestiona los paquetes PWOSPF
 *
 *---------------------------------------------------------------------*/

void sr_handle_pwospf_packet(struct sr_instance* sr, uint8_t* packet, unsigned int length, struct sr_if* rx_if)
{
    ospfv2_hdr_t* rx_ospfv2_hdr = ((ospfv2_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)));
    powspf_rx_lsu_param_t* rx_lsu_param = ((powspf_rx_lsu_param_t*)(malloc(sizeof(powspf_rx_lsu_param_t))));

    Debug("-> PWOSPF: Detecting PWOSPF Packet\n");
    Debug("      [Type = %d]\n", rx_ospfv2_hdr->type);

    switch(rx_ospfv2_hdr->type)
    {
        case OSPF_TYPE_HELLO:
            sr_handle_pwospf_hello_packet(sr, packet, length, rx_if);
            break;
        case OSPF_TYPE_LSU:
            rx_lsu_param->sr = sr;
            unsigned int i;
            for (i = 0; i < length; i++)
            {
                rx_lsu_param->packet[i] = packet[i];
            }
            rx_lsu_param->length = length;
            rx_lsu_param->rx_if = rx_if;
            pthread_create(&g_rx_lsu_thread, NULL, sr_handle_pwospf_lsu_packet, rx_lsu_param);
            break;
    }
} /* -- sr_handle_pwospf_packet -- */
