#include "net.h"

typedef struct {
    int active;
    net_device_config_t config;
} net_device_slot_t;

static net_device_slot_t devices[JOSH_NET_MAX_DEVICES];
static net_receive_fn upper_receiver;
static void *upper_receiver_context;
static net_stats_t stats;

static void zero_bytes(void *memory, size_t length) {
    uint8_t *p = (uint8_t *)memory;
    while (length--) *p++ = 0;
}

static void copy_bytes(void *destination, const void *source, size_t length) {
    uint8_t *d = (uint8_t *)destination;
    const uint8_t *s = (const uint8_t *)source;
    while (length--) *d++ = *s++;
}

static int bytes_equal(const void *left, const void *right, size_t length) {
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    while (length--) {
        if (*a++ != *b++) return 0;
    }
    return 1;
}

void net_init(void) {
    zero_bytes(devices, sizeof(devices));
    zero_bytes(&stats, sizeof(stats));
    upper_receiver = 0;
    upper_receiver_context = 0;
}

net_status_t net_register_device(const net_device_config_t *config, uint32_t *device_id_out) {
    if (!config || !device_id_out || !config->transmit || config->mtu == 0) {
        return NET_BAD_ARGUMENT;
    }

    for (uint32_t i = 0; i < JOSH_NET_MAX_DEVICES; ++i) {
        if (devices[i].active) continue;
        devices[i].active = 1;
        devices[i].config = *config;
        devices[i].config.name[sizeof(devices[i].config.name) - 1] = '\0';
        *device_id_out = i;
        stats.registered_devices++;
        return NET_OK;
    }
    return NET_NO_CAPACITY;
}

net_status_t net_send(uint32_t device_id, const void *frame, size_t length) {
    if (!frame || length == 0) return NET_BAD_ARGUMENT;
    if (device_id >= JOSH_NET_MAX_DEVICES || !devices[device_id].active) return NET_NOT_FOUND;

    const net_device_config_t *config = &devices[device_id].config;
    if (length > config->mtu + 14u || length > JOSH_NET_MAX_FRAME) {
        stats.dropped_frames++;
        return NET_FRAME_TOO_LARGE;
    }

    net_status_t status = config->transmit(config->context, (const uint8_t *)frame, length);
    if (status != NET_OK) {
        stats.dropped_frames++;
        return NET_DRIVER_ERROR;
    }

    stats.transmitted_frames++;
    stats.transmitted_bytes += length;
    return NET_OK;
}

net_status_t net_receive(uint32_t device_id, const void *frame, size_t length) {
    if (!frame || length == 0) return NET_BAD_ARGUMENT;
    if (device_id >= JOSH_NET_MAX_DEVICES || !devices[device_id].active) return NET_NOT_FOUND;
    if (length > devices[device_id].config.mtu + 14u || length > JOSH_NET_MAX_FRAME) {
        stats.dropped_frames++;
        return NET_FRAME_TOO_LARGE;
    }

    stats.received_frames++;
    stats.received_bytes += length;
    if (upper_receiver) {
        upper_receiver(device_id, (const uint8_t *)frame, length, upper_receiver_context);
    }
    return NET_OK;
}

void net_set_receiver(net_receive_fn receiver, void *context) {
    upper_receiver = receiver;
    upper_receiver_context = context;
}

net_stats_t net_stats(void) {
    return stats;
}

typedef struct {
    uint32_t device_id;
} loopback_context_t;

static loopback_context_t loopback_context;

static net_status_t loopback_transmit(void *context, const uint8_t *frame, size_t length) {
    loopback_context_t *loop = (loopback_context_t *)context;
    if (!loop) return NET_BAD_ARGUMENT;
    return net_receive(loop->device_id, frame, length);
}

net_status_t net_loopback_register(uint32_t *device_id_out) {
    if (!device_id_out) return NET_BAD_ARGUMENT;

    net_device_config_t config;
    zero_bytes(&config, sizeof(config));
    const char name[] = "loopback";
    copy_bytes(config.name, name, sizeof(name));
    config.mac[0] = 0x02;
    config.mac[5] = 0x01;
    config.mtu = 1500;
    config.context = &loopback_context;
    config.transmit = loopback_transmit;

    uint32_t id = 0;
    net_status_t status = net_register_device(&config, &id);
    if (status != NET_OK) return status;
    loopback_context.device_id = id;
    *device_id_out = id;
    return NET_OK;
}

typedef struct {
    int seen;
    uint32_t device_id;
    uint8_t bytes[64];
    size_t length;
} self_test_receive_t;

static void self_test_receiver(uint32_t device_id,
                               const uint8_t *frame,
                               size_t length,
                               void *context) {
    self_test_receive_t *capture = (self_test_receive_t *)context;
    if (!capture || length > sizeof(capture->bytes)) return;
    capture->seen = 1;
    capture->device_id = device_id;
    capture->length = length;
    copy_bytes(capture->bytes, frame, length);
}

int net_self_test(void) {
    net_init();

    uint32_t loopback = 0;
    if (net_loopback_register(&loopback) != NET_OK) return 0;

    self_test_receive_t capture;
    zero_bytes(&capture, sizeof(capture));
    net_set_receiver(self_test_receiver, &capture);

    const uint8_t frame[] = {
        0x02,0,0,0,0,1, 0x02,0,0,0,0,1, 0x08,0x00,
        0x45,0,0,20, 0,0,0,0, 64,1,0,0, 127,0,0,1, 127,0,0,1
    };

    if (net_send(loopback, frame, sizeof(frame)) != NET_OK) return 0;
    if (!capture.seen || capture.device_id != loopback || capture.length != sizeof(frame)) return 0;
    if (!bytes_equal(capture.bytes, frame, sizeof(frame))) return 0;

    net_stats_t current = net_stats();
    if (current.registered_devices != 1
        || current.transmitted_frames != 1
        || current.received_frames != 1
        || current.transmitted_bytes != sizeof(frame)
        || current.received_bytes != sizeof(frame)
        || current.dropped_frames != 0) {
        return 0;
    }

    net_set_receiver(0, 0);
    return 1;
}

const char *net_status_string(net_status_t status) {
    switch (status) {
        case NET_OK: return "ok";
        case NET_BAD_ARGUMENT: return "invalid argument";
        case NET_NO_CAPACITY: return "device table full";
        case NET_NOT_FOUND: return "network device not found";
        case NET_FRAME_TOO_LARGE: return "frame too large";
        case NET_DRIVER_ERROR: return "network driver error";
        default: return "unknown network error";
    }
}
