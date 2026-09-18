#ifndef JOSHOS_NET_PROTO_H
#define JOSHOS_NET_PROTO_H

#include <stddef.h>
#include <stdint.h>

#define JOSH_NET_ETHERTYPE_IPV4 0x0800u
#define JOSH_NET_ETHERTYPE_ARP  0x0806u

#define JOSH_NET_IP_ICMP 1u
#define JOSH_NET_IP_TCP  6u
#define JOSH_NET_IP_UDP  17u

typedef enum {
    JOSH_NET_OK = 0,
    JOSH_NET_ERR_ARGUMENT,
    JOSH_NET_ERR_TRUNCATED,
    JOSH_NET_ERR_MALFORMED,
    JOSH_NET_ERR_CHECKSUM,
    JOSH_NET_ERR_UNSUPPORTED,
    JOSH_NET_ERR_BUFFER
} josh_net_status_t;

typedef struct {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t ethertype;
    const uint8_t *payload;
    size_t payload_length;
} josh_eth_frame_t;

typedef struct {
    uint16_t operation;
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
} josh_arp_packet_t;

typedef struct {
    uint8_t dscp_ecn;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint8_t source[4];
    uint8_t destination[4];
    const uint8_t *payload;
    size_t payload_length;
    size_t header_length;
} josh_ipv4_packet_t;

typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t checksum;
    const uint8_t *payload;
    size_t payload_length;
} josh_udp_packet_t;

typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t acknowledgement;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    const uint8_t *payload;
    size_t payload_length;
    size_t header_length;
} josh_tcp_segment_t;

typedef struct {
    uint8_t message_type;
    uint8_t offered_ip[4];
    uint8_t server_id[4];
    uint8_t subnet_mask[4];
    uint8_t router[4];
    uint8_t dns[4];
    uint32_t lease_seconds;
} josh_dhcp_offer_t;

uint16_t josh_net_checksum(const void *data, size_t length);

josh_net_status_t josh_eth_parse(const void *frame, size_t length,
                                  josh_eth_frame_t *out);
size_t josh_eth_build(void *buffer, size_t capacity,
                      const uint8_t destination[6],
                      const uint8_t source[6],
                      uint16_t ethertype,
                      const void *payload, size_t payload_length);

josh_net_status_t josh_arp_parse(const void *payload, size_t length,
                                  josh_arp_packet_t *out);
size_t josh_arp_build_ethernet_ipv4(void *buffer, size_t capacity,
                                    uint16_t operation,
                                    const uint8_t sender_mac[6],
                                    const uint8_t sender_ip[4],
                                    const uint8_t target_mac[6],
                                    const uint8_t target_ip[4]);

josh_net_status_t josh_ipv4_parse(const void *packet, size_t length,
                                   josh_ipv4_packet_t *out);
size_t josh_ipv4_build(void *buffer, size_t capacity,
                       uint8_t protocol, uint8_t ttl,
                       uint16_t identification,
                       const uint8_t source[4],
                       const uint8_t destination[4],
                       const void *payload, size_t payload_length);

josh_net_status_t josh_icmp_echo_reply(const void *request, size_t request_length,
                                        void *reply, size_t capacity,
                                        size_t *reply_length);

josh_net_status_t josh_udp_parse(const void *segment, size_t length,
                                  josh_udp_packet_t *out);
size_t josh_udp_build_ipv4(void *buffer, size_t capacity,
                           const uint8_t source_ip[4],
                           const uint8_t destination_ip[4],
                           uint16_t source_port, uint16_t destination_port,
                           const void *payload, size_t payload_length);

josh_net_status_t josh_tcp_parse(const void *segment, size_t length,
                                  josh_tcp_segment_t *out);
uint16_t josh_tcp_checksum_ipv4(const uint8_t source_ip[4],
                                const uint8_t destination_ip[4],
                                const void *segment, size_t length);

josh_net_status_t josh_dhcp_parse_reply(const void *packet, size_t length,
                                         uint32_t expected_xid,
                                         josh_dhcp_offer_t *out);

size_t josh_dns_build_a_query(void *buffer, size_t capacity,
                              uint16_t transaction_id, const char *hostname);
josh_net_status_t josh_dns_parse_a_response(const void *packet, size_t length,
                                             uint16_t transaction_id,
                                             uint8_t address[4]);

const char *josh_net_status_string(josh_net_status_t status);

#endif
