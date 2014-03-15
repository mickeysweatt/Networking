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
#include <sr_rt.h>

/* 
  This function gets called every second. For each request sent out, we keep
  checking whether we should resend an request or destroy the arp request.
  See the comments in the header file for an idea of what it should look like.
*/

extern const int DEBUG;

static sr_arp_hdr_t* sr_arp_hdr_init_request(struct sr_instance *sr, 
                                             struct sr_arpreq   *req);
                                             
static void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req);


static void sr_arpcache_sweepreqs(struct sr_instance *sr);

static void handle_waiting_packets(struct sr_instance *sr, 
                                   struct sr_arpreq   *curr_req,
                                   int (* callback) (struct sr_instance* , 
                                                      uint8_t * , 
                                                      unsigned int , 
                                                      char*,
                                                      void *));
                                                      
struct sr_arpreq* find_request_for_ip(struct sr_arpcache *cache, 
                                      uint32_t            ip);

static void sr_arpcache_sweepreqs(struct sr_instance *sr)
{ 
    pthread_mutex_lock(&(sr->cache.lock));
    struct sr_arpreq *curr_req  = sr->cache.requests, *next_req = NULL;
    while (curr_req)
    {
        next_req = curr_req->next;
        pthread_mutex_unlock(&(sr->cache.lock));
        handle_arpreq(sr, curr_req);
        curr_req = next_req;
        pthread_mutex_lock(&(sr->cache.lock));
    }
    pthread_mutex_unlock(&(sr->cache.lock));
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
    struct sr_if *src_if = sr_get_interface(sr, req->packets->iface);
    memset(hdr, 0, sizeof(sr_arp_hdr_t));
    // Ethernet is 1
    hdr->ar_hrd = htons(arp_hrd_ethernet);
    hdr->ar_pro = 0x8;     /* format of protocol address   */
    // Ethernet is 6 bytes
    hdr->ar_hln = ETHER_ADDR_LEN;            /* length of hardware address   6*/
    // IPv4 is 4 bytes
    hdr->ar_pln = sizeof(hdr->ar_tip);       /* length of protocol address   4*/
    // 1 for request 2 for response
    hdr->ar_op  = htons(arp_op_request);     /* ARP opcode (command)         1*/
    memcpy(hdr->ar_sha, src_if->addr, ETHER_ADDR_LEN);
    hdr->ar_sip = src_if->ip;                 /* sender IP address            */
    
    hdr->ar_tip = req->ip;
    memset(&hdr->ar_tha, 0, ETHER_ADDR_LEN);
    return hdr;
}



void sr_handle_arp(struct sr_instance *sr, 
                   uint8_t            *packet/* lent */,
                   unsigned int        len,
                   char               *iface_name,
                   void               *parmas)
{
    uint8_t *arp_buf = 
           (uint8_t *) malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t));
    sr_ethernet_hdr_t *ether_hdr = (sr_ethernet_hdr_t *) arp_buf;
    sr_arp_hdr_t 	  *arp_hdr   = 
                    (sr_arp_hdr_t *)(arp_buf + sizeof(sr_ethernet_hdr_t));
    // copy the old packet (since the majority of the information is correct)
    memcpy(ether_hdr, packet, sizeof(sr_ethernet_hdr_t));
    memcpy(arp_hdr, packet + sizeof(sr_ethernet_hdr_t), sizeof(sr_arp_hdr_t));
    // If it is request -> ARP request processing    
    //print_hdrs(packet, len);
    if (arp_op_request == ntohs(arp_hdr->ar_op))
    {   
        if (DEBUG)
        {
            Debug("===Incomming ARP Request===\n");
            print_hdrs(packet, len);
        }
        // cache incomming informaiton as well (self-learning)
        if (0 != sr_arpcache_lookup(&sr->cache, arp_hdr->ar_sip))
        {
            sr_arpcache_insert(&sr->cache, arp_hdr->ar_sha, arp_hdr->ar_sip);
            printf("AFTER SELF LEARNED ARP\n");
        }
        struct sr_if *iface = sr_get_interface(sr, iface_name);
        // construct the Ethernet Header
        // set the destination ip to the original source
        memcpy(ether_hdr->ether_dhost, 
               ((sr_ethernet_hdr_t *)packet)->ether_shost, 
               ETHER_ADDR_LEN);
        // set the src mac to the mac for the corresponding IP
        memcpy(ether_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
        
        // construct the ARP request
        // set the operation to a REPLY
        arp_hdr->ar_op = htons(arp_op_reply);
        // the the source address to the mac of the correct interface
        memcpy(arp_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
        // set the target address (the destination) to the original source
        memcpy(arp_hdr->ar_tha, 
              ((sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)))->ar_sha, 
              ETHER_ADDR_LEN);
        // swap the source and destination ip
        swap(arp_hdr->ar_sip, arp_hdr->ar_tip);
        
        sr_send_packet(sr, 
                       arp_buf, 
                       sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t),
                       iface_name);
    }
    
    // If it is reply -> ARP reply processing 
    else if (arp_op_reply == ntohs(arp_hdr->ar_op))
    {
        if (DEBUG)
        {
            Debug("===Incomming ARP Response===\n");
            print_hdrs(packet, len);
        }
        if (sr_arpcache_insert(&sr->cache, arp_hdr->ar_sha, arp_hdr->ar_sip))
        {
            if (DEBUG) { Debug("Updated arp cache entry\n") };
            sr_arpcache_dump(&sr->cache);

        }
        struct sr_arpreq *req = find_request_for_ip(&sr->cache, arp_hdr->ar_sip);
        if (req)
        {
            handle_waiting_packets(sr, req, req->pass_handler);
        }
    }
    else
    {
        assert(0);
    }
}

struct sr_arpreq* find_request_for_ip(struct sr_arpcache *cache, 
                                      uint32_t            ip)
{
    struct sr_arpreq *curr_req = NULL;
    pthread_mutex_lock(&(cache->lock));
    for (curr_req  = cache->requests; 
         NULL     != curr_req; 
         curr_req  = curr_req->next)
    {
        if (ip == curr_req ->ip)
        {
            break;
        }
    }
    pthread_mutex_unlock(&(cache->lock));
    return curr_req;
}

static void handle_waiting_packets(struct sr_instance *sr,
                                   struct sr_arpreq * curr_req,
                                   int (* callback) (struct sr_instance* , 
                                                      uint8_t * , 
                                                      unsigned int , 
                                                      char*,
                                                      void *))
{
    struct sr_packet* curr_packet;
    pthread_mutex_lock(&(sr->cache.lock));
    for (curr_packet = curr_req->packets; 
         NULL       != curr_packet; 
         curr_packet = curr_packet->next)
    {
            callback(sr, 
                     curr_packet->buf, 
                     curr_packet->len, 
                     curr_packet->iface,
                     curr_req->handler_params);
    }
    if (!curr_req->packets || curr_req->packets == curr_packet)
    {
        fprintf(stderr, "%s:%d - No packets handled\n",__FILE__, __LINE__);
        return;
    }
    pthread_mutex_unlock(&(sr->cache.lock));
    sr_arpreq_destroy(&sr->cache, curr_req);
    
    return;
}
                             

void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req)
{
   time_t now = time(NULL);
   struct sr_arpcache *cache = &sr->cache;
   if (difftime(now, req->sent) > 1.0 || 0 == req->times_sent)
   {
       pthread_mutex_lock(&(cache->lock));
       if (req->times_sent >= 5)
       {
           // send icmp host unreachable to source addr of all
           // pkts waiting on this request
           Debug("---Dropping an arp request---\n");
           if (req->fail_handler)
           {
               handle_waiting_packets(sr, req, req->fail_handler);
           }
           else if (DEBUG)
           {
               Debug("%s : %d - No fail handler specified\n", __FILE__, __LINE__);
           }
       }
       else
       {
           sr_arp_hdr_t      *arp_req   = sr_arp_hdr_init_request(sr, req);
           struct sr_if *src_if = sr_get_interface(sr, req->packets->iface);
           uint8_t *arp_buf =  (uint8_t *) malloc(sizeof(sr_ethernet_hdr_t) + 
                                                  sizeof(sr_arp_hdr_t));
           sr_ethernet_hdr_t *ether_hdr = (sr_ethernet_hdr_t *)arp_buf;
           // copy the ethernet frame
           memcpy(ether_hdr, req->packets->buf, sizeof(sr_ethernet_hdr_t));
           ether_hdr->ether_type = htons(ethertype_arp);
           // set the ADDRESSES
           memset(ether_hdr->ether_dhost, ~0, ETHER_ADDR_LEN);
           memcpy(ether_hdr->ether_shost, src_if->addr, ETHER_ADDR_LEN);
           // copy in the arp request
           memcpy(arp_buf + sizeof(sr_ethernet_hdr_t), 
                  arp_req, 
                  sizeof(sr_arp_hdr_t));
           if (DEBUG) {Debug("====OUTGOING ARP REQUEST=====\n")};
           if (DEBUG) print_hdrs(arp_buf,sizeof(sr_ethernet_hdr_t) + 
                                         sizeof(sr_arp_hdr_t));
           sr_send_packet(sr, 
                          arp_buf, 
                          sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t),
                          req->packets->iface);
           req->sent = now;
           req->times_sent++;
           free(arp_buf);
       }
       pthread_mutex_unlock(&(cache->lock));
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
    for (i = 0; i < cache->num_valid_entries; i++) 
    {
        if ((cache->entries[i].valid) && (cache->entries[i].ip == ip)) 
        {
            entry = &(cache->entries[i]);
        }
    }
    
    /* Must return a copy b/c another thread could jump in and modify
       table after we return. */
    if (entry) 
    {
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
struct sr_arpreq *sr_arpcache_queuereq(struct sr_instance *sr,
                                       struct sr_arpcache *cache,
                                       uint32_t            ip,
                                       uint8_t            *packet,
                                       unsigned int        packet_len,
                                       char               *iface,
									   int(* pass_handler)(struct sr_instance*, 
															uint8_t * , 
															unsigned int , 
															char*,
                                                            void *),
									   int(* fail_handler) (struct sr_instance*, 
															uint8_t * , 
															unsigned int , 
															char* ,
                                                            void*),
                                       void               *params)
{
    
    
    struct sr_arpreq *req = find_request_for_ip(cache, ip);
    pthread_mutex_lock(&(cache->lock));
    /* If the IP wasn't found, add it */
    if (!req) 
    {
        req = (struct sr_arpreq *) calloc(1, sizeof(struct sr_arpreq));
        req->ip = ip;
        req->pass_handler   = pass_handler;
        req->fail_handler   = fail_handler;
        req->handler_params = params;
        if (req->pass_handler && pass_handler &&
            req->pass_handler != pass_handler)
        {
            fprintf(stderr, "PASS HANDLER OVERWRITTEN!\n");
            assert(0);
        }
        // if specified pass handler does not match previous one
        if (req->fail_handler && fail_handler &&
            req->fail_handler != fail_handler)
        {
            fprintf(stderr, "FAIL HANDLER OVERWRITTEN!\n");
            assert(0);
        }
        req->next = cache->requests;
        cache->requests = req;
    }
    
    /* Add the packet to the list of packets for this request */
    if (packet && packet_len && iface) 
    {
        struct sr_packet *new_pkt = 
                           (struct sr_packet *)malloc(sizeof(struct sr_packet));
        // if specified pass handler does not match previous one
        new_pkt->buf = (uint8_t *)malloc(packet_len);
        memcpy(new_pkt->buf, packet, packet_len);
        new_pkt->len = packet_len;
		new_pkt->iface = (char *)malloc(sr_IFACE_NAMELEN);
        strncpy(new_pkt->iface, iface, sr_IFACE_NAMELEN);
        new_pkt->next = req->packets;
        req->packets = new_pkt;
    }
    pthread_mutex_unlock(&(cache->lock));
    sr_arpcache_sweepreqs(sr);
    return req;
}

/* This method performs two functions:
   1) Looks up this IP in the request queue. If it is found, returns a pointer
      to the sr_arpreq with this IP. Otherwise, returns NULL.
   2) Inserts this IP to Msr_arpcache_insertAC mapping in the cache, and marks it valid. */
struct sr_arpreq *sr_arpcache_insert(struct sr_arpcache *cache,
                                     unsigned char      *mac,
                                     uint32_t            ip)
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
    for (i = 0; i < cache->num_valid_entries; i++) 
    {
        if (!(cache->entries[i].valid))
            break;
    }
    
    if (i != SR_ARPCACHE_SZ) {
        memcpy(cache->entries[i].mac, mac, 6);
        cache->entries[i].ip = ip;
        cache->entries[i].added = time(NULL);
        cache->entries[i].valid = 1;
        cache->num_valid_entries++;
    }
    pthread_mutex_unlock(&(cache->lock));
    
    return req;
}

/* Frees all memory associated with this arp request entry. If this arp request
   entry is on the arp request queue, it is removed from the queue. */
void sr_arpreq_destroy(struct sr_arpcache *cache, struct sr_arpreq *entry) 
{
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
void sr_arpcache_dump(struct sr_arpcache *cache) 
{
    fprintf(stderr, 
            "\nMAC            IP         ADDED                      VALID\n");
    fprintf(stderr, 
            "-----------------------------------------------------------\n");
    
    int i;
    pthread_mutex_lock(&(cache->lock));
    for (i = 0; i < cache->num_valid_entries; i++) 
    {
        
        struct sr_arpentry *cur = &(cache->entries[i]);
        unsigned char *mac = cur->mac;
        if(cur->valid)
        {
            fprintf(stderr, "%.1x%.1x%.1x%.1x%.1x%.1x   %.8x   %.24s   %d\n", 
                                                           mac[0],
                                                           mac[1],
                                                           mac[2],
                                                           mac[3],
                                                           mac[4],
                                                           mac[5],
                                                           ntohl(cur->ip),
                                                           ctime(&(cur->added)), 
                                                           cur->valid);
       }       
    }
    pthread_mutex_lock(&(cache->lock));
    fprintf(stderr, "\n");
}

/* Initialize table + table lock. Returns 0 on success. */
int sr_arpcache_init(struct sr_arpcache *cache) 
{  
    /* Seed RNG to kick out a random entry if all entries full. */
    srand(time(NULL));
    
    /* Invalidate all entries */
    memset(cache->entries, 0, sizeof(cache->entries));
    cache->requests = NULL;
    cache->num_valid_entries = 0;
    /* Acquire mutex lock */
    pthread_mutexattr_init(&(cache->attr));
    pthread_mutexattr_settype(&(cache->attr), PTHREAD_MUTEX_RECURSIVE);
    return pthread_mutex_init(&(cache->lock), &(cache->attr));
}

/* Destroys table + table lock. Returns 0 on success. */
int sr_arpcache_destroy(struct sr_arpcache *cache) 
{
    while(cache->num_valid_entries > 0)
    {
        //free(&cache->entries[0]);
        cache->num_valid_entries--;
    }
    struct sr_arpreq *curr = cache->requests, *next;
    while (curr)
    {
        next = curr->next;
        free(curr);
        curr = next;
    }
    return pthread_mutex_destroy(    &(cache->lock))
        && pthread_mutexattr_destroy(&(cache->attr));
}

/* Thread which sweeps through the cache and invalidates entries that were added
   more than SR_ARPCACHE_TO seconds ago. */
void *sr_arpcache_timeout(void *sr_ptr) 
{
    struct sr_instance *sr = sr_ptr;
    struct sr_arpcache *cache = &(sr->cache);
    
    while (1) 
    {
        sleep(1.0);
        
        pthread_mutex_lock(&(cache->lock));
    
        time_t curtime = time(NULL);
        
        int i;    
        for (i = 0; i < cache->num_valid_entries; i++) 
        {
            if ((cache->entries[i].valid) && 
                (difftime(curtime,cache->entries[i].added) > SR_ARPCACHE_TO)) 
            {
                cache->entries[i].valid = 0;
                // compact
                memcpy(&(cache->entries[i]), 
                       &(cache->entries[i+1]), 
                       (SR_ARPCACHE_SZ - i) * sizeof(struct sr_arpentry));
                i--;
                cache->num_valid_entries--;
            }
        }
        
        sr_arpcache_sweepreqs(sr);

        pthread_mutex_unlock(&(cache->lock));
    }
    
    return NULL;
}

