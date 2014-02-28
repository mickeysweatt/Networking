/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
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

static int sr_handle_ip(struct sr_instance *sr,
                        sr_ip_hdr_t         *ip_hdr,
                        sr_ethernet_hdr_t   *eth_hdr,
                        unsigned int        len,
                        char               *interface);

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr),       PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr),       PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);
    
    /* Add initialization code here! */

} /* -- sr_init -- */

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

void sr_handlepacket(struct sr_instance *sr,
                     uint8_t            *packet/* lent */,
                     unsigned int        len,
                     char               *interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */
  sr_ethernet_hdr_t *eth_hdr_p = NULL;
  sr_ip_hdr_t       *ip_hdr_p  = NULL;
  sr_arp_hdr_t      *arp_hdr_p = NULL;
  
  eth_hdr_p = (sr_ethernet_hdr_t *) malloc(sizeof(sr_ethernet_hdr_t));
  memcpy(eth_hdr_p, packet, sizeof(sr_ethernet_hdr_t));
  uint16_t ethtype = ethertype(packet);
  switch(ethtype)
  {
    case ethertype_ip:
    {
        ip_hdr_p  = malloc(sizeof(sr_ip_hdr_t));
        memcpy(ip_hdr_p, packet + sizeof(sr_ethernet_hdr_t), sizeof(sr_ip_hdr_t));
    } break;
    case ethertype_arp:
    {
        arp_hdr_p = malloc(sizeof(sr_arp_hdr_t));
        memcpy(arp_hdr_p, packet + sizeof(sr_ethernet_hdr_t), sizeof(sr_arp_hdr_t));
    } break;
    default:
    {
        // FIXME
        perror("ERROR");
    }
  }
  if (ip_hdr_p)
  {
    sr_handle_ip(sr, ip_hdr_p, eth_hdr_p, len, interface);
  }
  // clean-up
  if (ip_hdr_p)
  {
    free(ip_hdr_p);
  }
  if (eth_hdr_p)
  {
    free(eth_hdr_p);
  }
  if (arp_hdr_p)
  {
    free(arp_hdr_p);
  }

}/* end sr_ForwardPacket */

static int sr_handle_ip(struct sr_instance *sr,
                        sr_ip_hdr_t         *ip_hdr,
                        sr_ethernet_hdr_t   *eth_hdr,
                        unsigned int        len,
                        char               *interface)
{
    fprintf(stderr, "NOT IMPLEMENTED!!\n");
    // 1. Loop up dst_ip in routing table
    struct sr_rt *entry= sr_find_rt_entry(sr->routing_table, 
                                           ip_hdr->ip_dst);
    if (entry)
    {
        return -1;
    }
    // 1. That this point we have the interface to send it down
    return -1;
}   
