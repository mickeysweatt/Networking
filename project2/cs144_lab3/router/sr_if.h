/*-----------------------------------------------------------------------------
 * file:  sr_if.h
 * date:  Sun Oct 06 14:13:13 PDT 2002 
 * Contact: casado@stanford.edu 
 *
 * Description:
 *
 * Data structures and methods for handeling interfaces
 *
 *---------------------------------------------------------------------------*/

#ifndef sr_INTERFACE_H
#define sr_INTERFACE_H

#include <sr_protocol.h>

#ifdef _LINUX_
#include <stdint.h>
#endif /* _LINUX_ */

#ifdef _SOLARIS_
#include </usr/include/sys/int_types.h>
#endif /* SOLARIS */

#ifdef _DARWIN_
#include <inttypes.h>
#endif



struct sr_instance;

/* ----------------------------------------------------------------------------
 * struct sr_if
 *
 * Node in the interface list for each router
 *
 * -------------------------------------------------------------------------- */

struct sr_if
{
  char name[sr_IFACE_NAMELEN];                               // eth0
  unsigned char addr[ETHER_ADDR_LEN];                       // a0:21:b7:89:b5:b8
  uint32_t ip;                                              // 192.168.0.1
  uint32_t speed;                                           //
  struct sr_if* next;
};


void sr_init_interface(struct sr_instance*, const char*, uint32_t, const unsigned char*);
void sr_add_interface(struct sr_instance*, const char*);
void sr_set_ether_addr(struct sr_instance*, const unsigned char*);
void sr_set_ether_ip(struct sr_instance*, uint32_t ip_nbo);
struct sr_if* sr_get_interface(struct sr_instance* sr, const char* name);
int sr_address_is_valid(struct sr_instance* sr, const unsigned char*);
void sr_print_if_list(struct sr_instance*);
void sr_print_if(struct sr_if*);

#endif /* --  sr_INTERFACE_H -- */
