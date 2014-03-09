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
#include <sr_router.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sr_if.h>
#include <sr_rt.h>
#include <sr_protocol.h>
#include <sr_arpcache.h>
#include <sr_utils.h>

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/
extern const int DEBUG;

 void sr_init(struct sr_instance* sr, const char rtable_file[])
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
    sr_load_rt(sr, rtable_file);
    sr_arpcache_init(&(sr->cache));
    inet_aton("171.67.238.32", &sr->router_ip);
    //sr_arpcache_dump(&(sr->cache));
} /* -- sr_init -- */


/*---------------------------------------------------------------------
 * Method: sr_send_IP
 *--------------------------------------------------------------------*/
 int sr_send_IP(struct sr_instance *sr, 
                       sr_ip_hdr_t *ip_hdr_p)
 {
    // handle TTL
    if(ip_hdr_p->ip_ttl == 0 || (--(ip_hdr_p->ip_ttl) == 0))
    {
        // send ICMP Time exceeded
    }
    
    //ip_dst -> interface name
    struct sr_rt* rt_entry = sr_find_rt_entry(sr->routing_table, 
                                    ip_hdr_p->ip_dst);
    if(rt_entry == NULL)
    {
        // send ICMP network unreachable
    }
    
    //interface name -> physical addr
    struct sr_if* if_entry = sr_get_interface(sr, 
                                              rt_entry->interface);
    // if there is no entry for the interface that packet specifies
    if(if_entry == NULL)
    {
        return -1;
    }
    
    // ip_dst -> 
    struct sr_arpentry* arp_entry = sr_arpcache_lookup(&sr->cache, 
                                                       ip_hdr_p->ip_dst);												   
    if(arp_entry == NULL)
    {
        //send ARP request
    }
    
    //change up IP header
    //send
    
    return 0;
 }

/*---------------------------------------------------------------------
 * Method: sr_handle_IP
 * returns 0 or -1 depending on whether its a success or failure
 *--------------------------------------------------------------------*/

 int sr_handle_IP(struct sr_instance *sr,
                   uint8_t           *packet/* lent */,
                   unsigned int       len,
                   char              *interface/* lent */)
{   
	
	if(DEBUG) Debug("*** -> Handling IP packet\n");
	
	
	
	int min_length = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t);

	// FIXME Free these structures
	sr_ethernet_hdr_t *eth_hdr_p = NULL;
	sr_ip_hdr_t       *ip_hdr_p  = NULL;
	eth_hdr_p                    = (sr_ethernet_hdr_t *) 
                                              malloc(sizeof(sr_ethernet_hdr_t));
	ip_hdr_p                     = (sr_ip_hdr_t       *) 
                                                   malloc(sizeof(sr_ip_hdr_t));
	
    // initially copy original packet into respective structures
	memcpy(eth_hdr_p, 
           packet, 
           sizeof(sr_ethernet_hdr_t));
	
    memcpy(ip_hdr_p,
           packet + sizeof(sr_ethernet_hdr_t),
           sizeof(sr_ip_hdr_t));	

	// Check if checksum is correct for packet
	uint16_t expected_cksum         = ip_hdr_p->ip_sum;
	ip_hdr_p->ip_sum                = 0;
	uint16_t calculated_cksum       = cksum(ip_hdr_p, sizeof(sr_ip_hdr_t));
	
	// Verify checksum. If fail: drop the packet
	if(expected_cksum != calculated_cksum)
	{
		fprintf(stderr, "Checksum error in packet");
		return -1;
	}
	
	if (DEBUG) 
    { 
        Debug("Passed Checksum\n");
    }
    Debug("---Incomming IP Packet---\n");
    print_hdr_ip((uint8_t *)ip_hdr_p);
    

	// Check destination IP 
	// If destined to router, what is the protocol field in IP header? 
	if(ip_hdr_p->ip_dst == sr->router_ip.s_addr)
	{
		switch(ip_hdr_p->ip_p) 
        {
          case ip_protocol_icmp: 
          {
            if (DEBUG) Debug("IP protocol icmp\n");
            
            // min_length = ether + ip
            min_length += sizeof(sr_icmp_hdr_t);
            if (len < min_length)
            {
                fprintf(stderr, "ICMP, length too small\n");
                // FIXME
                return -1;
            }
            // TODO: ICMP -> ICMP processing (e.g., echo request, echo reply)
          } break;
          default:
		  {
				// UDP, TCP -> ICMP port unreachable
          }
		}
    }        
    else
	{
		// Decrease TTL. If TTL = 0: ICMP Time exceed 
		if(0 == ip_hdr_p->ip_ttl || 0 == (--(ip_hdr_p->ip_ttl)))
		{
			// TODO validate regular sorts of packets
		}
		
		// Routing table lookup
		struct sr_rt* rt_entry = sr_find_rt_entry(sr->routing_table, 
												  ip_hdr_p->ip_dst);
		if(NULL == rt_entry)
		{
			// Send ICMP network unreachable
		}
		
        // Translate interface name to phys addr
        struct sr_if* if_entry = sr_get_interface(sr, rt_entry->interface);
        if(NULL == if_entry)
        {
            return -1;
        }
			
        // Translate destination IP to next hop IP
        struct sr_arpentry* arp_entry = 
                                      sr_arpcache_lookup(&sr->cache, 
                                                         rt_entry->dest.s_addr);
        if(arp_entry == NULL)
        {
            // Send ARP request
            sr_arpcache_queuereq(&sr->cache,
                                 rt_entry->dest.s_addr, 
                                 packet, 
                                 len, 
                                 rt_entry->interface);
            // because the arp request in asynchronous, we must poll to see if
            // the request is ready
        }					
		else
        {
            // Changing source and destination mac for next hop
            memcpy(eth_hdr_p->ether_shost, if_entry->addr, ETHER_ADDR_LEN);
            memcpy(eth_hdr_p->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);
            // Recalc cksum
            ip_hdr_p->ip_sum = cksum(ip_hdr_p, sizeof(sr_ip_hdr_t));
            // Make the outgoing packet
            uint8_t *packet_out = (uint8_t *) malloc(len);
            memcpy(packet_out, eth_hdr_p, sizeof(sr_ethernet_hdr_t));
            memcpy(packet_out + sizeof(sr_ethernet_hdr_t), 
                   ip_hdr_p, 
                   sizeof(sr_ip_hdr_t));
            memcpy(packet_out + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t),
                   packet     + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t),
                   len -     (sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)));
            sr_send_packet(sr, 
                           packet_out, 
                           len,
                           rt_entry->interface);
           Debug("===Outgoing IP Packet===\n");
           print_hdrs(packet_out, len);
           
        }
	}
	
/*
	// routing table look-up

	// Routing entry is not found -> ICMP network unreachable 

	// Routing entry found, get the IP of next hop, look up in ARP table 

	// No ARP entry, send ARP request

	// If get ARP reply -> process IP packet relying on it. 

	// Found ARP entry, use it as dst MAC address, use outgoing interface 
	// MAC as src MAC address, send IP packet
*/
	return 0;
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

void sr_handlepacket(struct sr_instance *sr,
                     uint8_t            *packet/* lent */,
                     unsigned int        len,
                     char               *interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  Debug("*** -> Received packet of length %d \n",len);
    
  /* fill in code here */
  
  // Receive a packet 
  int min_length = sizeof(sr_ethernet_hdr_t);
  
  // checks if the length of packet is as long as ethernet header
  if (len < min_length) 
  {
	fprintf(stderr, "Ethernet, length too small\n");
    return;
  }
   // TODO Check dst MAC

  // Check the type of the ethenet packet
  switch(ethertype(packet))
  {
    case ethertype_ip:
    {
		min_length += sizeof(sr_ip_hdr_t);
		//check if length of packet is as long as ethernet + IP header
		if(len < min_length)
		{
			fprintf(stderr, "IP, length too small\n");
			return;
		}
		sr_handle_IP(sr, packet, len, interface);
    } break;
    case ethertype_arp:
    {
		min_length += sizeof(sr_arp_hdr_t);
		//check if length of packet is as long as ethernet + arp header
		//check if length of packet is as long as ethernet + arp header
		if(len < min_length)
		{
			fprintf(stderr, "ARP, length too small");
			return;
		}
        sr_handle_arp(sr, packet, len, interface);
    } break;
    default:
    {
        // FIXME
        perror("ERROR");
    }
  }

}
