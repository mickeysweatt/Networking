#include <sr_arpcache.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sr_router.h>
#include <sr_if.h>
#include <sr_protocol.h>
#include <sr_utils.h>
#include <sr_rt.h>
#include <limits.h>

int makeICMP_hdr(uint8_t* buf, sr_icmp_t3_hdr_t *s, enum sr_icmp_type t1, enum sr_icmp_code c1);

int makeIP_hdr(struct sr_instance *sr, sr_ip_hdr_t* ip, const char* interface, uint8_t* buf);

int makeEthr_hdr(struct sr_instance *sr, sr_ethernet_hdr_t* et, uint8_t* buf, enum sr_icmp_type type, enum sr_icmp_code code, char *iface);


int makeIP_hdr(struct sr_instance *sr, sr_ip_hdr_t* ip, const char* interface, uint8_t *buf)
{
   memset(ip, 0, sizeof(sr_ip_hdr_t));
   sr_ip_hdr_t *topOfIp = (sr_ip_hdr_t *) (buf + sizeof(sr_ethernet_hdr_t));
   memcpy(ip, topOfIp, sizeof(sr_ip_hdr_t));
   ip->ip_p = ip_protocol_icmp;
   ip->ip_v = ip_version_ipv4;
   ip->ip_len = htons(sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t));
   ip->ip_off = 0;
   ip->ip_ttl = INIT_TTL;
   ip->ip_id = (short)(rand() % USHRT_MAX);
   ip->ip_sum = 0;
   ip->ip_sum = cksum((void *)ip, sizeof(sr_ip_hdr_t));
  
   struct sr_if* i_face = sr_get_interface(sr, interface);
   ip->ip_src = i_face->ip;
   ip->ip_dst = topOfIp->ip_src;
   return 0;
}


int makeEthr_hdr(struct sr_instance *sr,
                 sr_ethernet_hdr_t  *et,
                 uint8_t            *buf, 
                 enum sr_icmp_type   type, 
                 enum sr_icmp_code   code,
                 char               *iface)
{
   memset(et, 0, sizeof(sr_ethernet_hdr_t));
   sr_ethernet_hdr_t* topOfEthr = (sr_ethernet_hdr_t*) (buf);
   sr_ip_hdr_t *ip_hdr_p  = (sr_ip_hdr_t *)(buf + sizeof(sr_ethernet_hdr_t));
   memcpy(et, topOfEthr, sizeof(sr_ethernet_hdr_t));
   memcpy(&(et->ether_shost), &(topOfEthr->ether_dhost), ETHER_ADDR_LEN);
   
   struct sr_rt* rt_entry = sr_find_rt_entry(sr->routing_table, ip_hdr_p->ip_src);
   struct sr_arpentry* arp_entry = sr_arpcache_lookup(&sr->cache, rt_entry->dest.s_addr);
   iface = rt_entry->interface;
   if (arp_entry == NULL)
   {
        uint8_t* parameters = malloc(sizeof(enum sr_icmp_type) + sizeof(enum sr_icmp_code));
        memcpy(parameters, &type, sizeof(type));
        memcpy(parameters + sizeof(type), &code, sizeof(code));
        uint8_t len = sizeof(sr_ip_hdr_t) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
        sr_arpcache_queuereq(sr,
                            &sr->cache,
                             rt_entry->dest.s_addr,
                             buf,
                             len,
                             rt_entry->interface,
                             sr_handle_ICMP,
                             NULL,
                             parameters);
        return -1;
    }
   else
   {
       memcpy(et->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);
       et->ether_type = htons(ethertype_ip);
   }
   return 0;
}


int makeICMP_hdr(uint8_t* buf, sr_icmp_t3_hdr_t *s, enum sr_icmp_type t1, enum sr_icmp_code c1)
{
   memset(s, 0, sizeof(sr_icmp_t3_hdr_t));
   s->icmp_type = t1; //set the type
   s->icmp_code = c1; //set the code to be Host Unreachable
   uint8_t *topOfIp = buf + sizeof(sr_ethernet_hdr_t);
   memcpy(&(s->data), topOfIp, ICMP_DATA_SIZE);
   s->icmp_sum = 0;
   s->icmp_sum = cksum((void *)s, sizeof(sr_icmp_t3_hdr_t));
   return 0;
}


sr_icmp_response_t* makeICMP_response(struct sr_instance *sr, 
                                      char         *interface, 
                                      uint8_t            *buf, 
                                      enum sr_icmp_type   t1, 
                                      enum sr_icmp_code   c1)
{
   sr_icmp_response_t* resp = malloc(sizeof(sr_icmp_response_t));
   memset(resp, 0, sizeof(sr_icmp_response_t));
   
   if (0 != makeEthr_hdr(sr, &resp->eth, buf, t1, c1, interface) ||
       0 != makeIP_hdr(sr, &resp->ip, interface, buf) ||
       0 != makeICMP_hdr(buf, &resp->s, t1, c1))
   {
        free(resp);
        return NULL;
   }
   return resp;
}
