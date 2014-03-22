#ifndef INCLUDED_ICMP_H
#define INCLUDED_ICMP_H

#include <sr_protocol.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>

struct sr_instance;

uint8_t *makeICMP_response(struct sr_instance *sr, 
                                      char               *interface, 
                                      uint8_t            *buf, 
                                      enum sr_icmp_type   t1, 
                                      enum sr_icmp_code   c1);

#endif // INCLUDED_ICMP_H