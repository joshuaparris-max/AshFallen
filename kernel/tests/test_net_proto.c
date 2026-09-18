#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/net_proto.h"

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void test_ethernet_arp(void) {
    const uint8_t src_mac[6] = {0x52,0x54,0x00,0x12,0x34,0x56};
    const uint8_t dst_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    const uint8_t src_ip[4] = {10,0,2,15};
    const uint8_t dst_ip[4] = {10,0,2,2};
    const uint8_t zero_mac[6] = {0};

    uint8_t arp[28];
    assert(josh_arp_build_ethernet_ipv4(
        arp, sizeof(arp), 1, src_mac, src_ip, zero_mac, dst_ip) == sizeof(arp));

    josh_arp_packet_t parsed_arp;
    assert(josh_arp_parse(arp, sizeof(arp), &parsed_arp) == JOSH_NET_OK);
    assert(parsed_arp.operation == 1);
    assert(memcmp(parsed_arp.sender_mac, src_mac, 6) == 0);
    assert(memcmp(parsed_arp.target_ip, dst_ip, 4) == 0);

    uint8_t frame[64];
    size_t frame_len = josh_eth_build(
        frame, sizeof(frame), dst_mac, src_mac, JOSH_NET_ETHERTYPE_ARP, arp, sizeof(arp));
    assert(frame_len == 42);

    josh_eth_frame_t parsed_frame;
    assert(josh_eth_parse(frame, frame_len, &parsed_frame) == JOSH_NET_OK);
    assert(parsed_frame.ethertype == JOSH_NET_ETHERTYPE_ARP);
    assert(parsed_frame.payload_length == sizeof(arp));
    assert(memcmp(parsed_frame.source, src_mac, 6) == 0);
    assert(josh_eth_parse(frame, 13, &parsed_frame) == JOSH_NET_ERR_TRUNCATED);
}

static void test_ipv4_icmp(void) {
    uint8_t request[12] = {8,0,0,0,0x12,0x34,0,1,'p','i','n','g'};
    put16(request + 2, josh_net_checksum(request, sizeof(request)));
    assert(josh_net_checksum(request, sizeof(request)) == 0);

    uint8_t reply[12];
    size_t reply_len = 0;
    assert(josh_icmp_echo_reply(request, sizeof(request), reply, sizeof(reply), &reply_len) == JOSH_NET_OK);
    assert(reply_len == sizeof(request));
    assert(reply[0] == 0);
    assert(josh_net_checksum(reply, reply_len) == 0);

    const uint8_t src[4] = {192,168,1,10};
    const uint8_t dst[4] = {192,168,1,1};
    uint8_t packet[64];
    size_t packet_len = josh_ipv4_build(
        packet, sizeof(packet), JOSH_NET_IP_ICMP, 64, 0x1234, src, dst, request, sizeof(request));
    assert(packet_len == 32);

    josh_ipv4_packet_t ip;
    assert(josh_ipv4_parse(packet, packet_len, &ip) == JOSH_NET_OK);
    assert(ip.protocol == JOSH_NET_IP_ICMP);
    assert(ip.ttl == 64);
    assert(ip.payload_length == sizeof(request));
    assert(memcmp(ip.source, src, 4) == 0);

    packet[10] ^= 1;
    assert(josh_ipv4_parse(packet, packet_len, &ip) == JOSH_NET_ERR_CHECKSUM);
}

static void test_udp(void) {
    const uint8_t src[4] = {10,0,2,15};
    const uint8_t dst[4] = {8,8,8,8};
    const uint8_t payload[] = {'d','n','s'};
    uint8_t segment[64];
    size_t length = josh_udp_build_ipv4(
        segment, sizeof(segment), src, dst, 49152, 53, payload, sizeof(payload));
    assert(length == 11);

    josh_udp_packet_t udp;
    assert(josh_udp_parse(segment, length, &udp) == JOSH_NET_OK);
    assert(udp.source_port == 49152);
    assert(udp.destination_port == 53);
    assert(udp.payload_length == sizeof(payload));
    assert(udp.checksum != 0);
    segment[5] = 7;
    assert(josh_udp_parse(segment, length, &udp) == JOSH_NET_ERR_MALFORMED);
}

static void test_tcp(void) {
    const uint8_t src[4] = {10,0,2,15};
    const uint8_t dst[4] = {93,184,216,34};
    uint8_t tcp[20] = {0};
    put16(tcp, 40000);
    put16(tcp + 2, 443);
    put32(tcp + 4, 0x01020304);
    tcp[12] = 0x50;
    tcp[13] = 0x02;
    put16(tcp + 14, 65535);
    put16(tcp + 16, josh_tcp_checksum_ipv4(src, dst, tcp, sizeof(tcp)));
    assert(josh_tcp_checksum_ipv4(src, dst, tcp, sizeof(tcp)) == 0);

    josh_tcp_segment_t parsed;
    assert(josh_tcp_parse(tcp, sizeof(tcp), &parsed) == JOSH_NET_OK);
    assert(parsed.source_port == 40000);
    assert(parsed.destination_port == 443);
    assert(parsed.sequence == 0x01020304u);
    assert(parsed.flags == 0x02);
    assert(parsed.header_length == 20);
}

static void test_dhcp(void) {
    uint8_t packet[300] = {0};
    packet[0] = 2;
    packet[1] = 1;
    packet[2] = 6;
    put32(packet + 4, 0xaabbccddu);
    packet[16] = 10; packet[17] = 0; packet[18] = 2; packet[19] = 15;
    put32(packet + 236, 0x63825363u);
    size_t o = 240;
    packet[o++] = 53; packet[o++] = 1; packet[o++] = 2;
    packet[o++] = 54; packet[o++] = 4;
    packet[o++] = 10; packet[o++] = 0; packet[o++] = 2; packet[o++] = 2;
    packet[o++] = 1; packet[o++] = 4;
    packet[o++] = 255; packet[o++] = 255; packet[o++] = 255; packet[o++] = 0;
    packet[o++] = 3; packet[o++] = 4;
    packet[o++] = 10; packet[o++] = 0; packet[o++] = 2; packet[o++] = 2;
    packet[o++] = 6; packet[o++] = 4;
    packet[o++] = 10; packet[o++] = 0; packet[o++] = 2; packet[o++] = 3;
    packet[o++] = 51; packet[o++] = 4; put32(packet + o, 3600); o += 4;
    packet[o++] = 255;

    josh_dhcp_offer_t offer;
    assert(josh_dhcp_parse_reply(packet, o, 0xaabbccddu, &offer) == JOSH_NET_OK);
    assert(offer.message_type == 2);
    assert(offer.offered_ip[3] == 15);
    assert(offer.router[3] == 2);
    assert(offer.dns[3] == 3);
    assert(offer.lease_seconds == 3600);
    assert(josh_dhcp_parse_reply(packet, o, 1, &offer) == JOSH_NET_ERR_MALFORMED);
}

static void test_dns(void) {
    uint8_t query[256];
    size_t query_len = josh_dns_build_a_query(query, sizeof(query), 0x4242, "www.youtube.com");
    assert(query_len > 20);
    assert(query[0] == 0x42 && query[1] == 0x42);

    uint8_t response[512] = {0};
    memcpy(response, query, query_len);
    response[2] = 0x81;
    response[3] = 0x80;
    put16(response + 6, 1);
    size_t o = query_len;
    response[o++] = 0xc0; response[o++] = 0x0c;
    put16(response + o, 1); o += 2;
    put16(response + o, 1); o += 2;
    put32(response + o, 60); o += 4;
    put16(response + o, 4); o += 2;
    response[o++] = 142; response[o++] = 250; response[o++] = 70; response[o++] = 206;

    uint8_t address[4];
    assert(josh_dns_parse_a_response(response, o, 0x4242, address) == JOSH_NET_OK);
    assert(address[0] == 142 && address[3] == 206);
    assert(josh_dns_parse_a_response(response, o, 0x1111, address) == JOSH_NET_ERR_MALFORMED);
    assert(josh_dns_build_a_query(query, 15, 1, "example.com") == 0);
}

int main(void) {
    test_ethernet_arp();
    test_ipv4_icmp();
    test_udp();
    test_tcp();
    test_dhcp();
    test_dns();
    puts("native network protocol tests passed");
    return 0;
}
