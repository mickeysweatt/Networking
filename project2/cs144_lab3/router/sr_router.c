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
#include <sr_icmp.h>
/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/
extern const int DEBUG;

static int sr_handle_IP(struct sr_instance *sr,

                          uint8_t          *packet/* lent */,
                          unsigned int      len,
                          char             *interface/* lent */,
                          void  	       *params);

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
 * Method: sr_create_ICMP
 *--------------------------------------------------------------------*/
static 
uint8_t* sr_create_ICMP_params(enum sr_icmp_type type, enum sr_icmp_code code)
{
    uint8_t* parameters = malloc(sizeof(enum sr_icmp_type) + sizeof(enum sr_icmp_code));
    memcpy(parameters, &type, sizeof(type));
    memcpy(parameters + sizeof(type), &code, sizeof(code));
	return parameters;
}


/*---------------------------------------------------------------------
 * Method: icmp_cksum
 * Checks the ICMP checksum before sending the packet out
 *--------------------------------------------------------------------*/
 int icmp_cksum(uint8_t *packet, unsigned int len)
 {
    if (DEBUG) { Debug("=====Checking ICMP Checksum=====\n"); }
    
    // Allocates memory for ICMP packet + data after it
    sr_icmp_t3_hdr_t* icmp_hdr_p = (sr_icmp_t3_hdr_t       *) 
                                    malloc(len - 
                                           sizeof(sr_ethernet_hdr_t) - 
                                           sizeof(sr_ip_hdr_t));
    
    // Unpacks ICMP packet + data into pointer
    memcpy(icmp_hdr_p, 
           packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t),
           len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
    
    // Extracts expected checksum and calculates checksum
    uint16_t expected_icmp_cksum = icmp_hdr_p->icmp_sum;
    icmp_hdr_p->icmp_sum = 0;
    uint16_t calculated_icmp_cksum = cksum((void *)icmp_hdr_p,
                                            len - 
                                            sizeof(sr_ethernet_hdr_t) - 
                                            sizeof(sr_ip_hdr_t));
    // Compares checksums
    if(expected_icmp_cksum != calculated_icmp_cksum)
    {
        if (DEBUG) { Debug("=====ICMP header checksum is wrong=====\n"); }
        return -1;
    }
    return 0;
}                                         

/*---------------------------------------------------------------------
 * Method: sr_handle_IP
 * returns 0 or -1 depending on whether its a success or failure
 *--------------------------------------------------------------------*/

static int sr_handle_IP(struct sr_instance *sr,
						uint8_t            *packet/* lent */,
						unsigned int        len,
						char               *interface,/* lent */
                        void               *params)
{   
	
	if(DEBUG) { Debug("*** -> Handling IP packet\n"); }
	
    int status = 0;
	int min_length = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t);

	sr_ethernet_hdr_t  *eth_hdr_p      = NULL;
	sr_ip_hdr_t        *ip_hdr_p       = NULL;
    uint8_t            *packet_out     = NULL;
    struct sr_arpentry *arp_entry      = NULL;
    uint8_t            *parameters     = NULL;
    
	eth_hdr_p                      = (sr_ethernet_hdr_t *) 
                                              malloc(sizeof(sr_ethernet_hdr_t));
	ip_hdr_p                       = (sr_ip_hdr_t       *) 
                                              malloc(sizeof(sr_ip_hdr_t));
	
    // Initially copy original packet into respective structures
	memcpy(eth_hdr_p, 
           packet, 
           sizeof(sr_ethernet_hdr_t));
    memcpy(ip_hdr_p,
           packet + sizeof(sr_ethernet_hdr_t),
           sizeof(sr_ip_hdr_t));	
           
   // Cache incomming informaiton as well (self-learning)
    if (0 == sr_arpcache_lookup(&sr->cache, ip_hdr_p->ip_src) &&
    	0 != sr_find_rt_entry(sr->routing_table,ip_hdr_p->ip_dst) )
    {
        sr_arpcache_insert(&sr->cache, eth_hdr_p->ether_shost, ip_hdr_p->ip_src);
        if (DEBUG) 
        {
            printf("AFTER SELF LEARNED ARP IN IP\n");
            sr_arpcache_dump(&sr->cache);
        }
    }
    
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
    
    // Put back original checksum
    ip_hdr_p->ip_sum = expected_cksum;
    
    // Checks if ICMP
    if(ip_hdr_p->ip_p == ip_protocol_icmp && -1 == icmp_cksum(packet, len))
    {
        return -1;
    }

	// Check destination IP 
	if(ip_hdr_p->ip_dst == sr->router_ip.s_addr)
	{
		switch(ip_hdr_p->ip_p) 
        {
          case ip_protocol_icmp: 
          {
            if (DEBUG) Debug("IP protocol icmp\n");
            
            // Min_length is ip header + ethernet header
            min_length += sizeof(sr_icmp_hdr_t);
            if (len < min_length)
            {
                fprintf(stderr, "ICMP, length too small\n");
                return -1;
            }
            
            //Create ICMP echo reply
            parameters = sr_create_ICMP_params(icmp_type_echo_reply,
                                               icmp_code_echo_reply);
            return sr_handle_ICMP(sr, 
                                  packet, 
                                  len, 
                                  interface, 
                                  (void *)parameters); 
          } break;
          default:
		  {
            // Create ICMP destination port unreachable
            parameters = 
                sr_create_ICMP_params(icmp_type_destination_port_unreachable,
                                      icmp_code_destination_port_unreachable);
            return sr_handle_ICMP(sr, 
                                  packet, 
                                  len, 
                                  interface, 
                                  (void *)parameters);
          }
		}
    }        
    else
	{
		// Check TTL 
		if(0 == ip_hdr_p->ip_ttl || 1 == ip_hdr_p->ip_ttl)
		{
            ip_hdr_p->ip_ttl--;
            ip_hdr_p->ip_sum = 0;
            ip_hdr_p->ip_sum = cksum(ip_hdr_p, sizeof(sr_ip_hdr_t));
            parameters = 
                  sr_create_ICMP_params(icmp_type_TLL_expired,
                                        icmp_code_TLL_expired);
            return sr_handle_ICMP(sr, 
                                  packet, 
                                  len, 
                                  interface, 
                                  (void *)parameters);
		}
		
		// Routing table lookup
		struct sr_rt* rt_entry = sr_find_rt_entry(sr->routing_table, 
												  ip_hdr_p->ip_dst);
		if(NULL == rt_entry)
		{
			// Send ICMP network unreachable
            parameters = 
                  sr_create_ICMP_params(icmp_type_destination_network_unreachable,
                                        icmp_code_destination_network_unreachable);
            return sr_handle_ICMP(sr, 
                                  packet, 
                                  len, 
                                  interface, 
                                  (void *)parameters);
		}
		
        // Translate interface name to phys addr
        struct sr_if* if_entry = sr_get_interface(sr, rt_entry->interface);
        if(NULL == if_entry)
        {
            return -1;
        }
			
        // Translate destination IP to next hop IP
        arp_entry = sr_arpcache_lookup(&sr->cache, rt_entry->dest.s_addr);
        
        if(arp_entry == NULL)
        {
            // Initialize fail parameters for sr_arpcache_queuereq
            parameters = 
                  sr_create_ICMP_params(icmp_type_destination_host_unreachable,
                                        icmp_code_destination_host_unreachable);
                                        
            // Send ARP request
            sr_arpcache_queuereq(sr,
                                &sr->cache,
                                 rt_entry->dest.s_addr, 
                                 packet, 
                                 len, 
                                 rt_entry->interface,
								 sr_handle_IP,
								 sr_handle_ICMP,
                                 parameters);
        }					
		else
        {
            // Decrement TTL
            ip_hdr_p->ip_ttl--;
            // Changing source and destination mac for next hop
            memcpy(eth_hdr_p->ether_shost, if_entry->addr, ETHER_ADDR_LEN);
            memcpy(eth_hdr_p->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);
            ip_hdr_p->ip_tos = 0;
            // Recalculate checksum
            ip_hdr_p->ip_sum = 0;
            ip_hdr_p->ip_sum = cksum(ip_hdr_p, sizeof(sr_ip_hdr_t));
            // Make the outgoing packet
            packet_out = (uint8_t *) malloc(len);
            memcpy(packet_out, eth_hdr_p, sizeof(sr_ethernet_hdr_t));
            memcpy(packet_out + sizeof(sr_ethernet_hdr_t), 
                   ip_hdr_p, 
                   sizeof(sr_ip_hdr_t));
            memcpy(packet_out + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t),
                   packet     + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t),
                   len -     (sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)));
            // Send packet out
            status = sr_send_packet(sr, 
                                    packet_out, 
                                    len,
                                    rt_entry->interface);
           if (DEBUG)
           {
               Debug("===Outgoing IP Packet===\n");
               print_hdrs(packet_out, len);
           }
        }
	}
    
    // Free all allocated data
    if(eth_hdr_p)
        free(eth_hdr_p);
    if(ip_hdr_p)
        free(ip_hdr_p);
    if(packet_out)
        free(packet_out);
    if(arp_entry)
        free(arp_entry);        
    if(parameters)
        free(parameters);
	return status;
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

int sr_handlepacket(struct sr_instance *sr,
                     uint8_t           *packet/* lent */,
                     unsigned int       len,
                     char              *interface,/* lent */
                     void              *params)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  Debug("*** -> Received packet of length %d \n",len);
    
  /* fill in code here */
  
  
  int status     = 0; 
  int min_length = sizeof(sr_ethernet_hdr_t);
  // Checks if the length of packet is as long as ethernet header
  if (len < min_length) 
  {
	fprintf(stderr, "Ethernet, length too small\n");
    return -1;
  }
   // Output packet
   if (DEBUG)
   {    
        fprintf(stderr, "===Incoming Packet===\n");
        print_hdrs(packet, len);
   }
  // Check the type of the ethenet packet
  switch(ethertype(packet))
  {
    case ethertype_ip:
    {
		min_length += sizeof(sr_ip_hdr_t);
		// Check if length of packet is as long as ethernet + IP header
		if(len < min_length)
		{
			fprintf(stderr, "IP, length too small\n");
			return -1;
		}
		status = sr_handle_IP(sr, packet, len, interface, params);
    } break;
    case ethertype_arp:
    {
		min_length += sizeof(sr_arp_hdr_t);
		// Check if length of packet is as long as ethernet + arp header
		if(len < min_length)
		{
			fprintf(stderr, "ARP, length too small");
			return -1;
		}
        sr_handle_arp(sr, packet, len, interface, params);
    } break;
    default:
    {
        perror("ERROR");
        return -1;
    }
  }
  return status;
}

int sr_handle_ICMP(struct sr_instance *sr,
						uint8_t           *packet/* lent */,
						unsigned int       len,
						char              *interface,/* lent */
                        void              *parameters)
{
	enum sr_icmp_type type;
	enum sr_icmp_code code;
	memcpy(&type, parameters, sizeof(type));
	memcpy(&code, parameters + sizeof(type), sizeof(code));
	uint8_t *response = makeICMP_response(sr, 
                                         interface, 
                                         packet,
                                         type, 
                                         code); 

    if (!response)
    {
        return -1;
    }
    else
    {
        if (DEBUG)
        {
            Debug("===OUTGOING ICMP RESPONSE===\n");
            print_hdrs((uint8_t *)response, len);
        }
        size_t packet_len = sizeof(sr_ethernet_hdr_t) + 
                            sizeof(sr_ip_hdr_t)       + 
                            sizeof(sr_icmp_t3_hdr_t);
        return sr_send_packet(sr, 
                            (uint8_t *)response,
                            packet_len, 
                            interface);
    }
	
}
