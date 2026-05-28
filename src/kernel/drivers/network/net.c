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

#include "net.h"
#include "rtl8139.h"
#include "../../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

// our mac and ip address
static uint8_t our_mac[6];
static uint32_t our_ip = 0;  // 0 means unconfigured

// stores our ip address
void net_set_ip(uint32_t ip) {
    our_ip = ip;
}

// stores our mac address
void net_set_mac(uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        our_mac[i] = mac[i];
    }
}

// copies our mac from the rtl8139 device and prints it
void net_init(void) {
    rtl8139_device_t *dev = rtl8139_get_device();
    for (int i = 0; i < 6; i++) {
        our_mac[i] = dev->mac_address[i];
    }
    printf("[NET] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           our_mac[0], our_mac[1], our_mac[2], 
           our_mac[3], our_mac[4], our_mac[5]);
}

// calculates the internet checksum over a block of data (one's complement of 16-bit sum)
static uint16_t net_checksum(uint8_t *data, uint16_t length) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;
    
    // sum all 16-bit words
    for (uint16_t i = 0; i < length / 2; i++) {
        sum += *(ptr + i);
    }
    
    // add leftover byte if odd length
    if (length & 1) {
        sum += ((uint8_t *)data)[length - 1] << 8;
    }
    
    // fold carries
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    // return one's complement
    return ~sum;
}

// sends an arp request asking who has the target ip
void net_send_arp_request(uint32_t target_ip) {
    uint8_t packet[64];
    int offset = 0;
    
    // ethernet header
    eth_header_t *eth = (eth_header_t *)packet;
    // broadcast mac for arp request
    for (int i = 0; i < 6; i++) {
        eth->dest_mac[i] = 0xFF;
    }
    for (int i = 0; i < 6; i++) {
        eth->src_mac[i] = our_mac[i];
    }
    eth->ethertype = 0x0608;  // arp in little-endian (0x0806)
    offset += sizeof(eth_header_t);
    
    // arp header
    arp_header_t *arp = (arp_header_t *)(packet + offset);
    arp->hardware_type = 0x0100;        // ethernet (1 in big-endian)
    arp->protocol_type = 0x0008;        // ipv4 (0x0800 in big-endian)
    arp->hardware_addr_len = 6;
    arp->protocol_addr_len = 4;
    arp->operation = 0x0100;            // request (1 in big-endian)
    
    for (int i = 0; i < 6; i++) {
        arp->sender_mac[i] = our_mac[i];
    }
    arp->sender_ip = our_ip;
    
    // target mac is unknown (all zeros for arp request)
    for (int i = 0; i < 6; i++) {
        arp->target_mac[i] = 0;
    }
    arp->target_ip = target_ip;
    offset += sizeof(arp_header_t);
    
    // send packet
    printf("[NET] Sending ARP request for %d.%d.%d.%d\n",
           (target_ip >> 0) & 0xFF, (target_ip >> 8) & 0xFF,
           (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
    
    rtl8139_send_packet((uint8_t *)packet, offset);
}

// sends an icmp echo request (ping) to the target ip
void net_send_icmp_echo(uint32_t target_ip, uint16_t seq) {
    uint8_t packet[128];
    int offset = 0;
    
    // ethernet header
    eth_header_t *eth = (eth_header_t *)packet;
    // for now, use broadcast (in real implementation, would use arp result)
    for (int i = 0; i < 6; i++) {
        eth->dest_mac[i] = 0xFF;
    }
    for (int i = 0; i < 6; i++) {
        eth->src_mac[i] = our_mac[i];
    }
    eth->ethertype = 0x0008;  // ipv4 in little-endian (0x0800)
    offset += sizeof(eth_header_t);
    
    // ipv4 header
    ipv4_header_t *ipv4 = (ipv4_header_t *)(packet + offset);
    ipv4->version_ihl = 0x45;  // version 4, header length 5
    ipv4->dscp_ecn = 0;
    ipv4->total_length = 0x2400;  // 36 bytes in big-endian (8 + 20 + 8)
    ipv4->identification = 0;
    ipv4->flags_fragment_offset = 0;
    ipv4->ttl = 64;
    ipv4->protocol = IP_PROTOCOL_ICMP;
    ipv4->header_checksum = 0;
    ipv4->src_ip = our_ip;
    ipv4->dest_ip = target_ip;
    
    // calculate ip header checksum
    ipv4->header_checksum = net_checksum((uint8_t *)ipv4, sizeof(ipv4_header_t));
    offset += sizeof(ipv4_header_t);
    
    // icmp header
    icmp_header_t *icmp = (icmp_header_t *)(packet + offset);
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->identifier = 0x0001;
    icmp->sequence = seq;
    
    // add some echo data
    uint8_t *data = packet + offset + sizeof(icmp_header_t);
    for (int i = 0; i < 8; i++) {
        data[i] = i;
    }
    offset += sizeof(icmp_header_t) + 8;
    
    // calculate icmp checksum
    icmp->checksum = net_checksum((uint8_t *)icmp, offset - (offset - sizeof(icmp_header_t) - 8));
    
    printf("[NET] Sending ICMP echo request to %d.%d.%d.%d\n",
           (target_ip >> 0) & 0xFF, (target_ip >> 8) & 0xFF,
           (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
    
    rtl8139_send_packet((uint8_t *)packet, offset);
}

// handles a received ethernet packet by checking its type
void net_handle_packet(uint8_t *packet, uint16_t length) {
    if (length < sizeof(eth_header_t)) {
        return;
    }
    
    eth_header_t *eth = (eth_header_t *)packet;
    uint16_t ethertype = eth->ethertype;
    
    // handle arp
    if (ethertype == 0x0608) {  // arp in little-endian
        if (length < sizeof(eth_header_t) + sizeof(arp_header_t)) {
            return;
        }
        arp_header_t *arp = (arp_header_t *)(packet + sizeof(eth_header_t));
        
        printf("[NET] ARP packet received\n");
    }
    
    // handle ipv4
    if (ethertype == 0x0008) {  // ipv4 in little-endian
        if (length < sizeof(eth_header_t) + sizeof(ipv4_header_t)) {
            return;
        }
        ipv4_header_t *ipv4 = (ipv4_header_t *)(packet + sizeof(eth_header_t));
        printf("[NET] IPv4 packet received\n");
    }
}
