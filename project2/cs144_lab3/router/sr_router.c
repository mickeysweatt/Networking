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
	
	printf("*** -> Handling IP packet\n");
	
	int min_length        = 0;

	// FIXME free this structure
	sr_ip_hdr_t* ip_hdr_p = NULL;
	ip_hdr_p              = malloc(sizeof(sr_ip_hdr_t));
	memcpy(ip_hdr_p, packet + sizeof(sr_ethernet_hdr_t), sizeof(sr_ip_hdr_t));	

	// check if checksum is correct for packet
	uint16_t expected_cksum         = ip_hdr_p->ip_sum;
	ip_hdr_p->ip_sum                = 0;
	uint16_t calculated_cksum       = cksum(ip_hdr_p, sizeof(sr_ip_hdr_t));
	
	printf("Expected cksum: %d\nCalculated_cksum: %d\n", 
	        expected_cksum, calculated_cksum);
	
	// Verify checksum. If fail: drop the packet
	if(expected_cksum != calculated_cksum)
	{
		fprintf(stderr, "Checksum error in packet");
		return -1;
	}

	//converts string IP to int IP
	struct in_addr sa;
	inet_aton("171.67.238.32", &sa);

	// check where IP packet is trying to go
	// Check destination IP 
	// If destined to router, what is the protocol field in IP header? 
	if(ip_hdr_p->ip_dst == sa.s_addr)
	{
		// ICMP -> ICMP processing (e.g., echo request, echo reply) 
		// UDP, TCP -> ICMP port unreachable    
	}

	// this IP packet is destined for somewhere else
	else
	{
		//FIXME
		return -1;
	}

	// check protocol field in IP header
	switch(ip_hdr_p->ip_p)
	{
		case ip_protocol_icmp:
		{
			min_length = sizeof(sr_icmp_hdr_t);
			//checks if length of packet is as long as ethernet + ICMP header
			if (len < min_length)
			{
				fprintf(stderr, "ICMP, length too small\n");
				// FIXME
				return -1;
			}
		}
		case ip_protocol_tcp:
		case ip_protocol_udp:
		// Decrease TTL. If TTL = 0: ICMP Time exceed 
		if(ip_hdr_p->ip_ttl == 0 || (--ip_hdr_p) == 0)
		{
			//TODO validate regular sorts of packets
		}
		default:
		{
			// TODO decide what to do here
		}
	}
	// Decrease TTL. If TTL = 0: ICMP Time exceed 
	if(ip_hdr_p->ip_ttl == 0 || (--ip_hdr_p) == 0)
	{
		//send ICMP Time exceeded
	}
	// routing table look-up

	// Routing entry is not found -> ICMP network unreachable 

	// Routing entry found, get the IP of next hop, look up in ARP table 

	// No ARP entry, send ARP request

	// If get ARP reply -> process IP packet relying on it. 

	// Found ARP entry, use it as dst MAC address, use outgoing interface 
	// MAC as src MAC address, send IP packet
	
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

  printf("*** -> Received packet of length %d \n",len);
    
  /* fill in code here */
  
  // Receive a packet 
 
  sr_ethernet_hdr_t *eth_hdr_p = NULL;
  sr_ip_hdr_t       *ip_hdr_p  = NULL;
  sr_arp_hdr_t      *arp_hdr_p = NULL;
  
  int min_length = sizeof(sr_ethernet_hdr_t);
  
  // checks if the length of packet is as long as ethernet header
  if (len < min_length) 
  {
	fprintf(stderr, "Ethernet, length too small\n");
    return;
  }
  
  
  eth_hdr_p = (sr_ethernet_hdr_t *) malloc(sizeof(sr_ethernet_hdr_t));
  memcpy(eth_hdr_p, packet, sizeof(sr_ethernet_hdr_t));
  uint16_t ethtype = ethertype(packet);	
  
  printf("Ethertype: %x\n", ethtype);
  // TODO Check dst MAC

  // Check the type of the ethenet packet
  switch(ethtype)
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
    } break;
    case ethertype_arp:
    {
		min_length += sizeof(sr_arp_hdr_t);
		//check if length of packet is as long as ethernet + arp header
		if(len < min_length)
		{
			fprintf(stderr, "ARP, length too small");
			return;
		}
        arp_hdr_p = malloc(sizeof(sr_arp_hdr_t));
		print_hdrs(packet, len);
		// hand spin arp response
		
		// hand spin ethernet
		uint8_t mac[] = {0x7a, 0x2e, 0x9f, 0xd4, 0xc3, 0x8b};
		uint8_t *buf = malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t));
		sr_ethernet_hdr_t *ether_hdr = buf;
		sr_arp_hdr_t 	  *arp_hdr   = buf + sizeof(sr_ethernet_hdr_t);
		memcpy(ether_hdr, packet, sizeof(sr_ethernet_hdr_t));
		memcpy(ether_hdr->ether_dhost, ((sr_ethernet_hdr_t *)packet)->ether_shost, ETHER_ADDR_LEN);
		memcpy(ether_hdr->ether_shost, mac, ETHER_ADDR_LEN);
		
		// hand spin arp
        memcpy(arp_hdr, packet + sizeof(sr_ethernet_hdr_t), sizeof(sr_arp_hdr_t));
		arp_hdr->ar_op = htons(0x2);
		memcpy(arp_hdr->ar_sha, mac, ETHER_ADDR_LEN);
		memcpy(arp_hdr->ar_tha, ((sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)))->ar_sha, ETHER_ADDR_LEN);
		arp_hdr->ar_sip = ((sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)))->ar_tip;
		arp_hdr->ar_tip = ((sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)))->ar_sip;
		char if_name[] = "eth3";
		sr_send_packet(sr, buf, sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t), if_name);
    } break;
    default:
    {
        // FIXME
        perror("ERROR");
    }
  }
  if (ip_hdr_p)
  {
    sr_handle_IP(sr, packet, len, interface);
  }
  else if (arp_hdr_p)
  {
	//sr_handle_arp(
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

}
