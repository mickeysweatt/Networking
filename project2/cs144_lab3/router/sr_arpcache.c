#include <sr_arpcache.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>

#include <sr_router.h>
#include <sr_if.h>
#include <sr_protocol.h>
#include <sr_utils.h>

/* 
  This function gets called every second. For each request sent out, we keep
  checking whether we should resend an request or destroy the arp request.
  See the comments in the header file for an idea of what it should look like.
*/
uint16_t cksum(const void *_data, int len);

static sr_arp_hdr_t* sr_arp_hdr_init_request(struct sr_instance *sr, 
                                             struct sr_arpreq   *req);
                                             
void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req);

void sr_arpcache_sweepreqs(struct sr_instance *sr) 
{ 
    struct sr_arpreq *curr_req  = sr->cache.requests, *next_req = NULL;
    while (curr_req)
    {
        next_req = curr_req->next;
        handle_arpreq(sr, curr_req);
        curr_req = next_req;
    }
}
/*
    This initializes a raw arp request with the target ip address of the 
*/


static 
sr_arp_hdr_t* sr_arp_hdr_init_request(struct sr_instance *sr, 
                                      struct sr_arpreq   *req)
{
    assert(req);
   
    sr_arp_hdr_t *hdr = malloc(sizeof(sr_arp_hdr_t));
    memset(req, 0, sizeof(sr_arp_hdr_t));
    // because all packets are attempting reach the same interface
    struct sr_if *src_if = 
                      (struct sr_if *)sr_get_interface(sr, req->packets->iface);
    // Ethernet is 1
    hdr->ar_hrd = arp_hrd_ethernet;
    hdr->ar_pro = ip_version_ipv4;            /* format of protocol address   */
    // Ethernet is 6 bytes
    hdr->ar_hln = ETHER_ADDR_LEN;            /* length of hardware address   6*/
    // IPv4 is 4 bytes
    hdr->ar_pln = sizeof(hdr->ar_tip);       /* length of protocol address   4*/
    // 1 for request 2 for response
    hdr->ar_op  = arp_op_request;           /* ARP opcode (command)         1*/
    memcpy(&(hdr->ar_sha), &(src_if->addr), ETHER_ADDR_LEN);
    hdr->ar_sip = src_if->ip;                 /* sender IP address            */
    memset(&(hdr->ar_sha), ~0, ETHER_ADDR_LEN);
    hdr->ar_tip = req->ip;
    return hdr;
}

sr_ip_hdr_t* makeIP_hdr(uint8_t *buf)
{
   sr_ip_hdr_t *ip = malloc(sizeof(sr_ip_hdr_t));
   memset(ip, 0, sizeof(sr_ip_hdr_t));
   ip->ip_p = ip_protocol_icmp;
   ip->ip_v = ip_version_ipv4;
   ip->ip_tos = 1;
   ip->ip_len = sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
   ip->ip_id = 0;
   ip->ip_off = 0;
   ip->ip_ttl = INIT_TTL;
   ip->ip_sum = cksum((void *)ip, sizeof(sr_ip_hdr_t));
   sr_ip_hdr_t *topOfIp = (sr_ip_hdr_t *) (buf + sizeof(sr_ethernet_hdr_t));
  // sr_ip_hdr_t *topOfIp = (sr_ip_hdr_t *) (req->packets->buf + sizeof(sr_ethernet_hdr_t));  
   ip->ip_src = topOfIp->ip_dst;
   ip->ip_dst = topOfIp->ip_src;
   return ip;    
}

sr_ethernet_hdr_t* makeEthr_hdr(uint8_t* buf)
{
   sr_ethernet_hdr_t* et = malloc(sizeof(sr_ethernet_hdr_t));
   memset(et, 0, sizeof(sr_ethernet_hdr_t));
   sr_ethernet_hdr_t* topOfEthr = (sr_ethernet_hdr_t*) (buf);
   memcpy(&(et->ether_shost), &(topOfEthr->ether_dhost), ETHER_ADDR_LEN);
   memcpy(&(et->ether_dhost), &(topOfEthr->ether_shost), ETHER_ADDR_LEN); 
   et->ether_type = ethertype_ip;
   return et;
}

sr_icmp_t3_hdr_t* makeICMP_hdr(uint8_t* buf)
{
   sr_icmp_t3_hdr_t *s = malloc(sizeof(sr_icmp_t3_hdr_t));
   memset(s, 0, sizeof(sr_icmp_t3_hdr_t));
   s->icmp_type = 3; //set the type
   s->icmp_code = 1; //set the code to be Host Unreachable
   uint8_t *topOfIp = buf + sizeof(sr_ethernet_hdr_t);
   memcpy(s->data, topOfIp, ICMP_DATA_SIZE);
   s->icmp_sum = cksum((void *)s->data, ICMP_DATA_SIZE);
   return s;
}

sr_icmp_response_t* makeICMP_response(uint8_t* buf)
{
   sr_icmp_response_t* resp = malloc(sizeof(sr_icmp_response_t));
   memset(resp, 0, sizeof(sr_icmp_response_t));
   resp->eth = makeEthr_hdr(buf);
   resp->ip = makeIP_hdr(buf);
   resp->s = makeICMP_hdr(buf);
   return resp;
}


void sr_handle_arp(struct sr_instance *sr, 
                   uint8_t            *packet/* lent */,
                   unsigned int        len,
                   char               *iface_name)
{
    printf("NOT IMPLEMENTED\n");
    print_hdrs(packet, len);
    uint8_t *arp_buf = 
           (uint8_t *) malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t));
    sr_ethernet_hdr_t *ether_hdr = (sr_ethernet_hdr_t *) arp_buf;
    sr_arp_hdr_t 	  *arp_hdr   = 
                    (sr_arp_hdr_t *)(arp_buf + sizeof(sr_ethernet_hdr_t));
    memcpy(ether_hdr, packet, sizeof(sr_ethernet_hdr_t));
    memcpy(arp_hdr, packet + sizeof(sr_ethernet_hdr_t), sizeof(sr_arp_hdr_t));
    // If it is request -> ARP request processing    
    if (arp_op_request == ntohs(arp_hdr->ar_op))
    {   
        struct sr_if *iface = sr_get_interface(sr, iface_name);
        
        memcpy(ether_hdr->ether_dhost, 
               ((sr_ethernet_hdr_t *)packet)->ether_shost, 
               ETHER_ADDR_LEN);
        memcpy(ether_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
        
        // hand spin arp
        arp_hdr->ar_op = htons(0x2);
        memcpy(arp_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
        memcpy(arp_hdr->ar_tha, 
              ((sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)))->ar_sha, 
              ETHER_ADDR_LEN);
        arp_hdr->ar_sip = 
                 ((sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)))->ar_tip;
        arp_hdr->ar_tip = 
                 ((sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)))->ar_sip;
        Debug("Sending:\n");
        print_hdrs(packet, len);
        sr_send_packet(sr, arp_buf, sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t), iface_name);
    }
    
    // If it is reply -> ARP reply processing 
    else if (arp_op_reply == ntohs(arp_hdr->ar_op))
    {
        if (!sr_arpcache_insert(&sr->cache, arp_hdr->ar_sha, arp_hdr->ar_sip))
        {
            Debug("Updated arp cache entry\n");
        }
    }
    else
    {
        // icmp response?
    }
}

void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req)
{
   time_t now = time(NULL);
   if (difftime(now, req->sent) > 1.0)
   {
       if (req->times_sent >= 5)
       {
            sr_icmp_response_t* resp = makeICMP_response(sr, req);
           // send icmp host unreachable to source addr of all
           // pkts waiting on this request
           sr_arpreq_destroy(&(sr->cache), req);
       }
       else
       {
           sr_arp_hdr_t* arp_req = sr_arp_hdr_init_request(sr, req);
           print_hdr_arp((uint8_t *)arp_req);
           // send arp request;
          
           req->sent = now;
           req->times_sent++;
       }
   }
}
/* You should not need to touch the rest of this code. */

/* Checks if an IP->MAC mapping is in the cache. IP is in network byte order.
   You must free the returned structure if it is not NULL. */
struct sr_arpentry *sr_arpcache_lookup(struct sr_arpcache *cache, 
                                       uint32_t            ip)
{
    

    pthread_mutex_lock(&(cache->lock));
    
    struct sr_arpentry *entry = NULL, *copy = NULL;
    
    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        if ((cache->entries[i].valid) && (cache->entries[i].ip == ip)) {
            entry = &(cache->entries[i]);
        }
    }
    
    /* Must return a copy b/c another thread could jump in and modify
       table after we return. */
    if (entry) {
        copy = (struct sr_arpentry *) malloc(sizeof(struct sr_arpentry));
        memcpy(copy, entry, sizeof(struct sr_arpentry));
    }
        
    pthread_mutex_unlock(&(cache->lock));
    
    return copy;
}

/* Adds an ARP request to the ARP request queue. If the request is already on
   the queue, adds the packet to the linked list of packets for this sr_arpreq
   that corresponds to this ARP request. You should free the passed *packet.
   
   A pointer to the ARP request is returned; it should not be freed. The caller
   can remove the ARP request from the queue by calling sr_arpreq_destroy. */
struct sr_arpreq *sr_arpcache_queuereq(struct sr_arpcache *cache,
                                       uint32_t ip,
                                       uint8_t *packet,           /* borrowed */
                                       unsigned int packet_len,
                                       char *iface)
{
    pthread_mutex_lock(&(cache->lock));
    
    struct sr_arpreq *req;
    for (req = cache->requests; req != NULL; req = req->next) {
        if (req->ip == ip) {
            break;
        }
    }
    
    /* If the IP wasn't found, add it */
    if (!req) {
        req = (struct sr_arpreq *) calloc(1, sizeof(struct sr_arpreq));
        req->ip = ip;
        req->next = cache->requests;
        cache->requests = req;
    }
    
    /* Add the packet to the list of packets for this request */
    if (packet && packet_len && iface) {
        struct sr_packet *new_pkt = (struct sr_packet *)malloc(sizeof(struct sr_packet));
        
        new_pkt->buf = (uint8_t *)malloc(packet_len);
        memcpy(new_pkt->buf, packet, packet_len);
        new_pkt->len = packet_len;
		new_pkt->iface = (char *)malloc(sr_IFACE_NAMELEN);
        strncpy(new_pkt->iface, iface, sr_IFACE_NAMELEN);
        new_pkt->next = req->packets;
        req->packets = new_pkt;
    }
    
    pthread_mutex_unlock(&(cache->lock));
    
    return req;
}

/* This method performs two functions:
   1) Looks up this IP in the request queue. If it is found, returns a pointer
      to the sr_arpreq with this IP. Otherwise, returns NULL.
   2) Inserts this IP to MAC mapping in the cache, and marks it valid. */
struct sr_arpreq *sr_arpcache_insert(struct sr_arpcache *cache,
                                     unsigned char *mac,
                                     uint32_t ip)
{
    pthread_mutex_lock(&(cache->lock));
    
    struct sr_arpreq *req, *prev = NULL, *next = NULL; 
    for (req = cache->requests; req != NULL; req = req->next) {
        if (req->ip == ip) {            
            if (prev) {
                next = req->next;
                prev->next = next;
            } 
            else {
                next = req->next;
                cache->requests = next;
            }
            
            break;
        }
        prev = req;
    }
    
    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        if (!(cache->entries[i].valid))
            break;
    }
    
    if (i != SR_ARPCACHE_SZ) {
        memcpy(cache->entries[i].mac, mac, 6);
        cache->entries[i].ip = ip;
        cache->entries[i].added = time(NULL);
        cache->entries[i].valid = 1;
    }
    
    pthread_mutex_unlock(&(cache->lock));
    
    return req;
}

/* Frees all memory associated with this arp request entry. If this arp request
   entry is on the arp request queue, it is removed from the queue. */
void sr_arpreq_destroy(struct sr_arpcache *cache, struct sr_arpreq *entry) {
    pthread_mutex_lock(&(cache->lock));
    
    if (entry) {
        struct sr_arpreq *req, *prev = NULL, *next = NULL; 
        for (req = cache->requests; req != NULL; req = req->next) {
            if (req == entry) {                
                if (prev) {
                    next = req->next;
                    prev->next = next;
                } 
                else {
                    next = req->next;
                    cache->requests = next;
                }
                
                break;
            }
            prev = req;
        }
        
        struct sr_packet *pkt, *nxt;
        
        for (pkt = entry->packets; pkt; pkt = nxt) {
            nxt = pkt->next;
            if (pkt->buf)
                free(pkt->buf);
            if (pkt->iface)
                free(pkt->iface);
            free(pkt);
        }
        
        free(entry);
    }
    
    pthread_mutex_unlock(&(cache->lock));
}

/* Prints out the ARP table. */
void sr_arpcache_dump(struct sr_arpcache *cache) {
    fprintf(stderr, "\nMAC            IP         ADDED                      VALID\n");
    fprintf(stderr, "-----------------------------------------------------------\n");
    
    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) 
    {
        struct sr_arpentry *cur = &(cache->entries[i]);
        unsigned char *mac = cur->mac;
        fprintf(stderr, "%.1x%.1x%.1x%.1x%.1x%.1x   %.8x   %.24s   %d\n", mac[0],
                                                                          mac[1],
                                                                          mac[2],
                                                                          mac[3],
                                                                          mac[4],
                                                                          mac[5],
                                                                          ntohl(cur->ip),
                                                                          ctime(&(cur->added)), 
                                                                          cur->valid);
    }
    
    fprintf(stderr, "\n");
}

/* Initialize table + table lock. Returns 0 on success. */
int sr_arpcache_init(struct sr_arpcache *cache) {  
    /* Seed RNG to kick out a random entry if all entries full. */
    srand(time(NULL));
    
    /* Invalidate all entries */
    memset(cache->entries, 0, sizeof(cache->entries));
    cache->requests = NULL;
    
    /* Acquire mutex lock */
    pthread_mutexattr_init(&(cache->attr));
    pthread_mutexattr_settype(&(cache->attr), PTHREAD_MUTEX_RECURSIVE);
    int success = pthread_mutex_init(&(cache->lock), &(cache->attr));
    
    return success;
}

/* Destroys table + table lock. Returns 0 on success. */
int sr_arpcache_destroy(struct sr_arpcache *cache) {
    return pthread_mutex_destroy(&(cache->lock)) && pthread_mutexattr_destroy(&(cache->attr));
}

/* Thread which sweeps through the cache and invalidates entries that were added
   more than SR_ARPCACHE_TO seconds ago. */
void *sr_arpcache_timeout(void *sr_ptr) {
    struct sr_instance *sr = sr_ptr;
    struct sr_arpcache *cache = &(sr->cache);
    
    while (1) {
        sleep(1.0);
        
        pthread_mutex_lock(&(cache->lock));
    
        time_t curtime = time(NULL);
        
        int i;    
        for (i = 0; i < SR_ARPCACHE_SZ; i++) {
            if ((cache->entries[i].valid) && (difftime(curtime,cache->entries[i].added) > SR_ARPCACHE_TO)) {
                cache->entries[i].valid = 0;
            }
        }
        
        sr_arpcache_sweepreqs(sr);

        pthread_mutex_unlock(&(cache->lock));
    }
    
    return NULL;
}
