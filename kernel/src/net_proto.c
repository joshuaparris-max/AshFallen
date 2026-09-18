#include "net_proto.h"

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_be16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void write_be32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void copy_bytes(uint8_t *destination, const uint8_t *source, size_t length) {
    while (length--) *destination++ = *source++;
}

static void zero_bytes(uint8_t *destination, size_t length) {
    while (length--) *destination++ = 0;
}

static uint32_t checksum_add(uint32_t sum, const uint8_t *data, size_t length) {
    while (length >= 2) {
        sum += read_be16(data);
        data += 2;
        length -= 2;
    }
    if (length) sum += (uint16_t)((uint16_t)data[0] << 8);
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return sum;
}

static uint16_t checksum_finish(uint32_t sum) {
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return (uint16_t)~sum;
}

uint16_t josh_net_checksum(const void *data, size_t length) {
    if (!data && length != 0) return 0;
    return checksum_finish(checksum_add(0, (const uint8_t *)data, length));
}

josh_net_status_t josh_eth_parse(const void *frame, size_t length,
                                  josh_eth_frame_t *out) {
    if (!frame || !out) return JOSH_NET_ERR_ARGUMENT;
    if (length < 14) return JOSH_NET_ERR_TRUNCATED;
    const uint8_t *p = (const uint8_t *)frame;
    copy_bytes(out->destination, p, 6);
    copy_bytes(out->source, p + 6, 6);
    out->ethertype = read_be16(p + 12);
    out->payload = p + 14;
    out->payload_length = length - 14;
    return JOSH_NET_OK;
}

size_t josh_eth_build(void *buffer, size_t capacity,
                      const uint8_t destination[6],
                      const uint8_t source[6],
                      uint16_t ethertype,
                      const void *payload, size_t payload_length) {
    if (!buffer || !destination || !source || (!payload && payload_length != 0)) return 0;
    if (payload_length > capacity || capacity - payload_length < 14) return 0;
    uint8_t *p = (uint8_t *)buffer;
    copy_bytes(p, destination, 6);
    copy_bytes(p + 6, source, 6);
    write_be16(p + 12, ethertype);
    if (payload_length) copy_bytes(p + 14, (const uint8_t *)payload, payload_length);
    return 14 + payload_length;
}

josh_net_status_t josh_arp_parse(const void *payload, size_t length,
                                  josh_arp_packet_t *out) {
    if (!payload || !out) return JOSH_NET_ERR_ARGUMENT;
    if (length < 28) return JOSH_NET_ERR_TRUNCATED;
    const uint8_t *p = (const uint8_t *)payload;
    if (read_be16(p) != 1 || read_be16(p + 2) != JOSH_NET_ETHERTYPE_IPV4
        || p[4] != 6 || p[5] != 4) {
        return JOSH_NET_ERR_UNSUPPORTED;
    }
    out->operation = read_be16(p + 6);
    if (out->operation != 1 && out->operation != 2) return JOSH_NET_ERR_MALFORMED;
    copy_bytes(out->sender_mac, p + 8, 6);
    copy_bytes(out->sender_ip, p + 14, 4);
    copy_bytes(out->target_mac, p + 18, 6);
    copy_bytes(out->target_ip, p + 24, 4);
    return JOSH_NET_OK;
}

size_t josh_arp_build_ethernet_ipv4(void *buffer, size_t capacity,
                                    uint16_t operation,
                                    const uint8_t sender_mac[6],
                                    const uint8_t sender_ip[4],
                                    const uint8_t target_mac[6],
                                    const uint8_t target_ip[4]) {
    if (!buffer || !sender_mac || !sender_ip || !target_mac || !target_ip) return 0;
    if (capacity < 28 || (operation != 1 && operation != 2)) return 0;
    uint8_t *p = (uint8_t *)buffer;
    write_be16(p, 1);
    write_be16(p + 2, JOSH_NET_ETHERTYPE_IPV4);
    p[4] = 6;
    p[5] = 4;
    write_be16(p + 6, operation);
    copy_bytes(p + 8, sender_mac, 6);
    copy_bytes(p + 14, sender_ip, 4);
    copy_bytes(p + 18, target_mac, 6);
    copy_bytes(p + 24, target_ip, 4);
    return 28;
}

josh_net_status_t josh_ipv4_parse(const void *packet, size_t length,
                                   josh_ipv4_packet_t *out) {
    if (!packet || !out) return JOSH_NET_ERR_ARGUMENT;
    if (length < 20) return JOSH_NET_ERR_TRUNCATED;
    const uint8_t *p = (const uint8_t *)packet;
    uint8_t version = (uint8_t)(p[0] >> 4);
    uint8_t ihl = (uint8_t)(p[0] & 0x0f);
    if (version != 4 || ihl < 5) return JOSH_NET_ERR_MALFORMED;
    size_t header_length = (size_t)ihl * 4u;
    if (header_length > length) return JOSH_NET_ERR_TRUNCATED;
    uint16_t total_length = read_be16(p + 2);
    if (total_length < header_length || total_length > length) return JOSH_NET_ERR_TRUNCATED;
    if (josh_net_checksum(p, header_length) != 0) return JOSH_NET_ERR_CHECKSUM;

    out->dscp_ecn = p[1];
    out->identification = read_be16(p + 4);
    out->flags_fragment = read_be16(p + 6);
    out->ttl = p[8];
    out->protocol = p[9];
    copy_bytes(out->source, p + 12, 4);
    copy_bytes(out->destination, p + 16, 4);
    out->payload = p + header_length;
    out->payload_length = total_length - header_length;
    out->header_length = header_length;
    return JOSH_NET_OK;
}

size_t josh_ipv4_build(void *buffer, size_t capacity,
                       uint8_t protocol, uint8_t ttl,
                       uint16_t identification,
                       const uint8_t source[4],
                       const uint8_t destination[4],
                       const void *payload, size_t payload_length) {
    if (!buffer || !source || !destination || (!payload && payload_length != 0)) return 0;
    if (payload_length > 65515u || capacity < 20u + payload_length) return 0;
    uint8_t *p = (uint8_t *)buffer;
    zero_bytes(p, 20);
    p[0] = 0x45;
    write_be16(p + 2, (uint16_t)(20u + payload_length));
    write_be16(p + 4, identification);
    write_be16(p + 6, 0x4000u);
    p[8] = ttl;
    p[9] = protocol;
    copy_bytes(p + 12, source, 4);
    copy_bytes(p + 16, destination, 4);
    write_be16(p + 10, josh_net_checksum(p, 20));
    if (payload_length) copy_bytes(p + 20, (const uint8_t *)payload, payload_length);
    return 20u + payload_length;
}

josh_net_status_t josh_icmp_echo_reply(const void *request, size_t request_length,
                                        void *reply, size_t capacity,
                                        size_t *reply_length) {
    if (!request || !reply || !reply_length) return JOSH_NET_ERR_ARGUMENT;
    if (request_length < 8) return JOSH_NET_ERR_TRUNCATED;
    if (capacity < request_length) return JOSH_NET_ERR_BUFFER;
    const uint8_t *src = (const uint8_t *)request;
    if (src[0] != 8 || src[1] != 0) return JOSH_NET_ERR_UNSUPPORTED;
    if (josh_net_checksum(src, request_length) != 0) return JOSH_NET_ERR_CHECKSUM;
    uint8_t *dst = (uint8_t *)reply;
    copy_bytes(dst, src, request_length);
    dst[0] = 0;
    dst[2] = 0;
    dst[3] = 0;
    write_be16(dst + 2, josh_net_checksum(dst, request_length));
    *reply_length = request_length;
    return JOSH_NET_OK;
}

static uint16_t transport_checksum_ipv4(const uint8_t source_ip[4],
                                        const uint8_t destination_ip[4],
                                        uint8_t protocol,
                                        const void *segment, size_t length) {
    uint32_t sum = 0;
    sum = checksum_add(sum, source_ip, 4);
    sum = checksum_add(sum, destination_ip, 4);
    uint8_t pseudo[4] = {0, protocol, (uint8_t)(length >> 8), (uint8_t)length};
    sum = checksum_add(sum, pseudo, sizeof(pseudo));
    sum = checksum_add(sum, (const uint8_t *)segment, length);
    return checksum_finish(sum);
}

josh_net_status_t josh_udp_parse(const void *segment, size_t length,
                                  josh_udp_packet_t *out) {
    if (!segment || !out) return JOSH_NET_ERR_ARGUMENT;
    if (length < 8) return JOSH_NET_ERR_TRUNCATED;
    const uint8_t *p = (const uint8_t *)segment;
    uint16_t encoded_length = read_be16(p + 4);
    if (encoded_length < 8 || encoded_length > length) return JOSH_NET_ERR_MALFORMED;
    out->source_port = read_be16(p);
    out->destination_port = read_be16(p + 2);
    out->checksum = read_be16(p + 6);
    out->payload = p + 8;
    out->payload_length = encoded_length - 8;
    return JOSH_NET_OK;
}

size_t josh_udp_build_ipv4(void *buffer, size_t capacity,
                           const uint8_t source_ip[4],
                           const uint8_t destination_ip[4],
                           uint16_t source_port, uint16_t destination_port,
                           const void *payload, size_t payload_length) {
    if (!buffer || !source_ip || !destination_ip || (!payload && payload_length != 0)) return 0;
    if (payload_length > 65527u || capacity < 8u + payload_length) return 0;
    uint8_t *p = (uint8_t *)buffer;
    write_be16(p, source_port);
    write_be16(p + 2, destination_port);
    write_be16(p + 4, (uint16_t)(8u + payload_length));
    write_be16(p + 6, 0);
    if (payload_length) copy_bytes(p + 8, (const uint8_t *)payload, payload_length);
    uint16_t checksum = transport_checksum_ipv4(
        source_ip, destination_ip, JOSH_NET_IP_UDP, p, 8u + payload_length);
    if (checksum == 0) checksum = 0xffffu;
    write_be16(p + 6, checksum);
    return 8u + payload_length;
}

josh_net_status_t josh_tcp_parse(const void *segment, size_t length,
                                  josh_tcp_segment_t *out) {
    if (!segment || !out) return JOSH_NET_ERR_ARGUMENT;
    if (length < 20) return JOSH_NET_ERR_TRUNCATED;
    const uint8_t *p = (const uint8_t *)segment;
    size_t header_length = (size_t)(p[12] >> 4) * 4u;
    if (header_length < 20 || header_length > length) return JOSH_NET_ERR_MALFORMED;
    out->source_port = read_be16(p);
    out->destination_port = read_be16(p + 2);
    out->sequence = read_be32(p + 4);
    out->acknowledgement = read_be32(p + 8);
    out->flags = p[13];
    out->window = read_be16(p + 14);
    out->checksum = read_be16(p + 16);
    out->payload = p + header_length;
    out->payload_length = length - header_length;
    out->header_length = header_length;
    return JOSH_NET_OK;
}

uint16_t josh_tcp_checksum_ipv4(const uint8_t source_ip[4],
                                const uint8_t destination_ip[4],
                                const void *segment, size_t length) {
    if (!source_ip || !destination_ip || (!segment && length != 0) || length > 65535u) return 0;
    return transport_checksum_ipv4(source_ip, destination_ip, JOSH_NET_IP_TCP, segment, length);
}

josh_net_status_t josh_dhcp_parse_reply(const void *packet, size_t length,
                                         uint32_t expected_xid,
                                         josh_dhcp_offer_t *out) {
    if (!packet || !out) return JOSH_NET_ERR_ARGUMENT;
    if (length < 240) return JOSH_NET_ERR_TRUNCATED;
    const uint8_t *p = (const uint8_t *)packet;
    if (p[0] != 2 || p[1] != 1 || p[2] != 6) return JOSH_NET_ERR_UNSUPPORTED;
    if (read_be32(p + 4) != expected_xid) return JOSH_NET_ERR_MALFORMED;
    if (read_be32(p + 236) != 0x63825363u) return JOSH_NET_ERR_MALFORMED;

    zero_bytes((uint8_t *)out, sizeof(*out));
    copy_bytes(out->offered_ip, p + 16, 4);

    size_t offset = 240;
    while (offset < length) {
        uint8_t code = p[offset++];
        if (code == 0) continue;
        if (code == 255) break;
        if (offset >= length) return JOSH_NET_ERR_TRUNCATED;
        uint8_t option_length = p[offset++];
        if ((size_t)option_length > length - offset) return JOSH_NET_ERR_TRUNCATED;
        const uint8_t *value = p + offset;
        if (code == 53 && option_length == 1) out->message_type = value[0];
        else if (code == 54 && option_length == 4) copy_bytes(out->server_id, value, 4);
        else if (code == 1 && option_length == 4) copy_bytes(out->subnet_mask, value, 4);
        else if (code == 3 && option_length >= 4) copy_bytes(out->router, value, 4);
        else if (code == 6 && option_length >= 4) copy_bytes(out->dns, value, 4);
        else if (code == 51 && option_length == 4) out->lease_seconds = read_be32(value);
        offset += option_length;
    }
    if (out->message_type != 2 && out->message_type != 5) return JOSH_NET_ERR_MALFORMED;
    return JOSH_NET_OK;
}

size_t josh_dns_build_a_query(void *buffer, size_t capacity,
                              uint16_t transaction_id, const char *hostname) {
    if (!buffer || !hostname || capacity < 17) return 0;
    uint8_t *p = (uint8_t *)buffer;
    zero_bytes(p, 12);
    write_be16(p, transaction_id);
    write_be16(p + 2, 0x0100u);
    write_be16(p + 4, 1);
    size_t out = 12;
    const char *label = hostname;
    for (;;) {
        const char *end = label;
        while (*end && *end != '.') ++end;
        size_t label_length = (size_t)(end - label);
        if (label_length == 0 || label_length > 63 || out + 1 + label_length + 5 > capacity) return 0;
        p[out++] = (uint8_t)label_length;
        for (size_t i = 0; i < label_length; ++i) p[out++] = (uint8_t)label[i];
        if (!*end) break;
        label = end + 1;
    }
    p[out++] = 0;
    write_be16(p + out, 1);
    write_be16(p + out + 2, 1);
    return out + 4;
}

static josh_net_status_t dns_skip_name(const uint8_t *p, size_t length,
                                        size_t *offset) {
    size_t pos = *offset;
    size_t labels = 0;
    while (pos < length) {
        uint8_t size = p[pos++];
        if (size == 0) {
            *offset = pos;
            return JOSH_NET_OK;
        }
        if ((size & 0xc0u) == 0xc0u) {
            if (pos >= length) return JOSH_NET_ERR_TRUNCATED;
            *offset = pos + 1;
            return JOSH_NET_OK;
        }
        if ((size & 0xc0u) != 0 || size > 63 || size > length - pos) return JOSH_NET_ERR_MALFORMED;
        pos += size;
        if (++labels > 127) return JOSH_NET_ERR_MALFORMED;
    }
    return JOSH_NET_ERR_TRUNCATED;
}

josh_net_status_t josh_dns_parse_a_response(const void *packet, size_t length,
                                             uint16_t transaction_id,
                                             uint8_t address[4]) {
    if (!packet || !address) return JOSH_NET_ERR_ARGUMENT;
    if (length < 12) return JOSH_NET_ERR_TRUNCATED;
    const uint8_t *p = (const uint8_t *)packet;
    if (read_be16(p) != transaction_id) return JOSH_NET_ERR_MALFORMED;
    uint16_t flags = read_be16(p + 2);
    if ((flags & 0x8000u) == 0 || (flags & 0x000fu) != 0) return JOSH_NET_ERR_MALFORMED;
    uint16_t questions = read_be16(p + 4);
    uint16_t answers = read_be16(p + 6);
    size_t offset = 12;

    for (uint16_t i = 0; i < questions; ++i) {
        josh_net_status_t status = dns_skip_name(p, length, &offset);
        if (status != JOSH_NET_OK) return status;
        if (length - offset < 4) return JOSH_NET_ERR_TRUNCATED;
        offset += 4;
    }

    for (uint16_t i = 0; i < answers; ++i) {
        josh_net_status_t status = dns_skip_name(p, length, &offset);
        if (status != JOSH_NET_OK) return status;
        if (length - offset < 10) return JOSH_NET_ERR_TRUNCATED;
        uint16_t type = read_be16(p + offset);
        uint16_t klass = read_be16(p + offset + 2);
        uint16_t rdlength = read_be16(p + offset + 8);
        offset += 10;
        if (rdlength > length - offset) return JOSH_NET_ERR_TRUNCATED;
        if (type == 1 && klass == 1 && rdlength == 4) {
            copy_bytes(address, p + offset, 4);
            return JOSH_NET_OK;
        }
        offset += rdlength;
    }
    return JOSH_NET_ERR_UNSUPPORTED;
}

const char *josh_net_status_string(josh_net_status_t status) {
    switch (status) {
        case JOSH_NET_OK: return "ok";
        case JOSH_NET_ERR_ARGUMENT: return "invalid argument";
        case JOSH_NET_ERR_TRUNCATED: return "truncated packet";
        case JOSH_NET_ERR_MALFORMED: return "malformed packet";
        case JOSH_NET_ERR_CHECKSUM: return "checksum failure";
        case JOSH_NET_ERR_UNSUPPORTED: return "unsupported packet";
        case JOSH_NET_ERR_BUFFER: return "buffer too small";
        default: return "unknown network error";
    }
}
