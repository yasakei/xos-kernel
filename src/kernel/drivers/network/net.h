// -------------------------------------------------------------------
// mit license
// 
// copyright (c) 2026 xos
// 
// permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "software"), to deal in the software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the software, and to permit persons to whom the
// software is furnished to do so, subject to the following
// conditions:
// 
// the above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the software.
// 
// the software is provided "as is", without warranty of any kind,
// express or implied, including but not limited to the warranties
// of merchantability, fitness for a particular purpose and
// noninfringement. in no event shall the authors or copyright
// holders be liable for any claim, damages or other liability,
// whether in an action of contract, tort or otherwise, arising
// from, out of or in connection with the software or the use or
// other dealings in the software.
// -------------------------------------------------------------------

#ifndef NET_H
#define NET_H

#include <stdint.h>

// ethernet header
typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

// ipv4 header
typedef struct {
    uint8_t version_ihl;           // version (4 bits) + header length (4 bits)
    uint8_t dscp_ecn;              // dscp (6 bits) + ecn (2 bits)
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ipv4_header_t;

// icmp header (echo request/reply)
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

// arp header
typedef struct {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_addr_len;
    uint8_t protocol_addr_len;
    uint16_t operation;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

// ethernet types
#define ETH_TYPE_ARP    0x0806
#define ETH_TYPE_IPv4   0x0800

// arp operations
#define ARP_REQUEST     1
#define ARP_REPLY       2

// icmp types
#define ICMP_ECHO_REPLY     0
#define ICMP_ECHO_REQUEST   8

// ip protocol numbers
#define IP_PROTOCOL_ICMP    1

// network utility functions
void net_init(void);
void net_set_ip(uint32_t ip);
void net_set_mac(uint8_t *mac);
void net_send_arp_request(uint32_t target_ip);
void net_send_icmp_echo(uint32_t target_ip, uint16_t seq);
void net_handle_packet(uint8_t *packet, uint16_t length);

#endif // NET_H
