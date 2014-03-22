#ifndef INCLUDED_ICMP_H
#define INCLUDED_ICMP_H

#include <sr_protocol.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>

struct sr_instance;

struct sr_icmp_response
{
   sr_ethernet_hdr_t eth;
   sr_ip_hdr_t ip;             /* IP header */
   sr_icmp_t3_hdr_t icmp;         /* ICMP header */
} __attribute__ ((packed)) ;
typedef struct sr_icmp_response sr_icmp_response_t;

uint8_t *makeICMP_response(struct sr_instance *sr, 
                                      char               *interface, 
                                      uint8_t            *buf, 
                                      enum sr_icmp_type   t1, 
                                      enum sr_icmp_code   c1);

#endif // INCLUDED_ICMP_H