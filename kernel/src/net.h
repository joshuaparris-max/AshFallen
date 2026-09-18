#ifndef JOSHOS_NET_H
#define JOSHOS_NET_H

#include <stddef.h>
#include <stdint.h>

#define JOSH_NET_MAX_DEVICES 8u
#define JOSH_NET_MAX_FRAME   1600u

typedef enum {
    NET_OK = 0,
    NET_BAD_ARGUMENT,
    NET_NO_CAPACITY,
    NET_NOT_FOUND,
    NET_FRAME_TOO_LARGE,
    NET_DRIVER_ERROR
} net_status_t;

typedef net_status_t (*net_transmit_fn)(
    void *context,
    const uint8_t *frame,
    size_t length
);

typedef void (*net_receive_fn)(
    uint32_t device_id,
    const uint8_t *frame,
    size_t length,
    void *context
);

typedef struct {
    char name[16];
    uint8_t mac[6];
    uint16_t mtu;
    void *context;
    net_transmit_fn transmit;
} net_device_config_t;

typedef struct {
    uint32_t registered_devices;
    uint64_t transmitted_frames;
    uint64_t transmitted_bytes;
    uint64_t received_frames;
    uint64_t received_bytes;
    uint64_t dropped_frames;
} net_stats_t;

void net_init(void);
net_status_t net_register_device(const net_device_config_t *config, uint32_t *device_id_out);
net_status_t net_send(uint32_t device_id, const void *frame, size_t length);
net_status_t net_receive(uint32_t device_id, const void *frame, size_t length);
void net_set_receiver(net_receive_fn receiver, void *context);
net_stats_t net_stats(void);

net_status_t net_loopback_register(uint32_t *device_id_out);
int net_self_test(void);
const char *net_status_string(net_status_t status);

#endif
