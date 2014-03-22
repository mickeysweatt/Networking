#include <sr_icmp.h>    
#include <sr_arpcache.h>
#include <sr_router.h>
#include <sr_if.h>
#include <sr_protocol.h>
#include <sr_utils.h>
#include <sr_rt.h>
#include <stdlib.h>
#include <limits.h>

//==============================================================================
//                          PRIVATE TYPES
//==============================================================================
struct sr_icmp_response
{
   sr_ethernet_hdr_t eth;
   sr_ip_hdr_t ip;             /* IP header */
   sr_icmp_t3_hdr_t icmp;         /* ICMP header */
} __attribute__ ((packed)) ;
typedef struct sr_icmp_response sr_icmp_response_t;

//==============================================================================
//                          LOCAL FUNCTION DECLARATIONS
//==============================================================================
static int makeICMP_hdr(uint8_t           *buf, 
                        sr_icmp_t3_hdr_t  *s, 
                        enum sr_icmp_type  type, 
                        enum sr_icmp_code  code);

static int makeIP_hdr(struct sr_instance *sr, 
                      uint32_t            ip_src, 
                      sr_ip_hdr_t        *ip, 
                      uint8_t            *buf);

static int makeEthr_hdr(struct sr_instance *sr,
                        sr_ethernet_hdr_t  *et,
                        unsigned char       mac[ETHER_ADDR_LEN],
                        uint8_t            *buf);
                
//==============================================================================
//                          LOCAL FUNCTION DEFINITIONS
//==============================================================================

static int makeEthr_hdr(struct sr_instance *sr,
                 sr_ethernet_hdr_t         *et,
                 unsigned char              mac[ETHER_ADDR_LEN],
                 uint8_t                   *buf)
{
   memset(et, 0, sizeof(sr_ethernet_hdr_t));
   sr_ethernet_hdr_t* topOfEthr = (sr_ethernet_hdr_t*) (buf);
   memcpy(et, topOfEthr, sizeof(sr_ethernet_hdr_t));
   memcpy(et->ether_shost, topOfEthr->ether_dhost, ETHER_ADDR_LEN);
   memcpy(et->ether_dhost, mac, ETHER_ADDR_LEN);
   et->ether_type = htons(ethertype_ip);
   return 0;
}

static int makeIP_hdr(struct sr_instance *sr, 
                      uint32_t           ip_src, 
                      sr_ip_hdr_t        *ip, 
                      uint8_t            *buf)
{
   sr_ip_hdr_t *ip_hdr_p = (sr_ip_hdr_t *) (buf + sizeof(sr_ethernet_hdr_t));
   memcpy(ip, ip_hdr_p, sizeof(sr_ip_hdr_t));
   ip->ip_p = ip_protocol_icmp;
   ip->ip_v = ip_version_ipv4;
   ip->ip_len = htons(sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t));
   ip->ip_off = 0;
   ip->ip_ttl = INIT_TTL;
   ip->ip_id = (short)(rand() % USHRT_MAX);
   ip->ip_sum = 0;
   ip->ip_src = ip_src;
   ip->ip_dst = ip_hdr_p->ip_src;
   ip->ip_sum = cksum((void *)ip, sizeof(sr_ip_hdr_t));
   return 0;
}

int makeICMP_hdr(uint8_t          *buf, 
                 sr_icmp_t3_hdr_t *s,
                 enum sr_icmp_type type, 
                 enum sr_icmp_code code)
{
   memset(s, 0, sizeof(sr_icmp_t3_hdr_t));
   s->icmp_type = type; //set the type
   s->icmp_code = code; //set the code to be Host Unreachable
   uint8_t *ip_hdr_p = buf + sizeof(sr_ethernet_hdr_t);
   memcpy(&(s->data), ip_hdr_p, ICMP_DATA_SIZE);
   s->icmp_sum = 0;
   s->icmp_sum = cksum((void *)s, sizeof(sr_icmp_t3_hdr_t));
   return 0;
}
//==============================================================================
//                          SR_ICMP DEFINTIONS
//==============================================================================
uint8_t* makeICMP_response(struct sr_instance *sr, 
                           char               *iface, 
                           uint8_t            *buf, 
                           enum sr_icmp_type   type, 
                           enum sr_icmp_code   code)
{
   sr_icmp_response_t *resp = malloc(sizeof(sr_ethernet_hdr_t) + 
                                     sizeof(sr_ip_hdr_t)       + 
                                     sizeof(sr_icmp_t3_hdr_t));
   memset(resp, 0, sizeof(sr_icmp_response_t));
   // Routing table look-up for destination
   sr_ip_hdr_t *ip_hdr_p  = (sr_ip_hdr_t *)(buf + sizeof(sr_ethernet_hdr_t));
   struct sr_rt *rt_entry = sr_find_rt_entry(sr->routing_table, 
                                             ip_hdr_p->ip_src);
   // interface look-up
   iface = rt_entry->interface;
   struct sr_if* i_face = sr_get_interface(sr, iface);
   
   // ARP Look-up
   struct sr_arpentry* arp_entry = sr_arpcache_lookup(&sr->cache, 
                                                       rt_entry->dest.s_addr);
   if (arp_entry == NULL)
   {
        uint8_t* parameters = malloc(sizeof(enum sr_icmp_type) + 
                                     sizeof(enum sr_icmp_code));
        memcpy(parameters, &type, sizeof(type));
        memcpy(parameters + sizeof(type), &code, sizeof(code));
        uint8_t len = sizeof(sr_ip_hdr_t)       + 
                      sizeof(sr_ethernet_hdr_t) + 
                      sizeof(sr_icmp_t3_hdr_t);
        sr_arpcache_queuereq(sr,
                            &sr->cache,
                             rt_entry->dest.s_addr,
                             buf,
                             len,
                             rt_entry->interface,
                             sr_handle_ICMP,
                             NULL,
                             parameters);
        return NULL;
   }
   else
   {
       if (0 != makeEthr_hdr(sr, &resp->eth, arp_entry->mac, buf) ||
           0 != makeIP_hdr(sr, i_face->ip, &resp->ip, buf)        ||
           0 != makeICMP_hdr(buf, &resp->icmp, type, code))
       {
            free(resp);
            return NULL;
       }
   }
   return (uint8_t*) resp;
}