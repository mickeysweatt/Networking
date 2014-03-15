#include <inttypes.h>
#include <time.h>
#include <pthread.h>
#include <sr_protocol.h>
#include <sr_arpcache.h>


struct sr_icmp_response;

sr_icmp_response_t* makeICMP_response(struct sr_instance *sr, const char* interface, uint8_t* buf, enum sr_icmp_type t1, enum sr_icmp_code c1);

