#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/net.h"

typedef struct {
    unsigned calls;
    uint8_t frame[64];
    size_t length;
} capture_t;

static net_status_t capture_transmit(void *context, const uint8_t *frame, size_t length) {
    capture_t *capture = (capture_t *)context;
    capture->calls++;
    capture->length = length;
    if (length <= sizeof(capture->frame)) memcpy(capture->frame, frame, length);
    return NET_OK;
}

typedef struct {
    unsigned calls;
    uint32_t device;
    uint8_t frame[64];
    size_t length;
} receive_capture_t;

static void receive_packet(uint32_t device_id, const uint8_t *frame,
                           size_t length, void *context) {
    receive_capture_t *capture = (receive_capture_t *)context;
    capture->calls++;
    capture->device = device_id;
    capture->length = length;
    if (length <= sizeof(capture->frame)) memcpy(capture->frame, frame, length);
}

static void test_device_send(void) {
    net_init();
    capture_t capture = {0};
    net_device_config_t config = {0};
    strcpy(config.name, "test0");
    config.mac[0] = 0x02;
    config.mtu = 1500;
    config.context = &capture;
    config.transmit = capture_transmit;

    uint32_t id = 99;
    assert(net_register_device(&config, &id) == NET_OK);
    assert(id == 0);

    uint8_t frame[32] = {0};
    assert(net_send(id, frame, sizeof(frame)) == NET_OK);
    assert(capture.calls == 1);
    assert(capture.length == sizeof(frame));

    net_stats_t stats = net_stats();
    assert(stats.registered_devices == 1);
    assert(stats.transmitted_frames == 1);
    assert(stats.transmitted_bytes == sizeof(frame));

    uint8_t huge[JOSH_NET_MAX_FRAME + 1] = {0};
    assert(net_send(id, huge, sizeof(huge)) == NET_FRAME_TOO_LARGE);
    assert(net_stats().dropped_frames == 1);
}

static void test_loopback(void) {
    net_init();
    uint32_t id = 99;
    assert(net_loopback_register(&id) == NET_OK);

    receive_capture_t capture = {0};
    net_set_receiver(receive_packet, &capture);

    const uint8_t frame[] = {1,2,3,4,5,6,7,8};
    assert(net_send(id, frame, sizeof(frame)) == NET_OK);
    assert(capture.calls == 1);
    assert(capture.device == id);
    assert(capture.length == sizeof(frame));
    assert(memcmp(capture.frame, frame, sizeof(frame)) == 0);

    net_stats_t stats = net_stats();
    assert(stats.transmitted_frames == 1);
    assert(stats.received_frames == 1);
    assert(net_self_test());
}

static void test_capacity_and_arguments(void) {
    net_init();
    uint32_t id;
    assert(net_register_device(NULL, &id) == NET_BAD_ARGUMENT);
    assert(net_send(0, NULL, 1) == NET_BAD_ARGUMENT);
    assert(net_send(0, "x", 1) == NET_NOT_FOUND);

    capture_t captures[JOSH_NET_MAX_DEVICES] = {0};
    for (uint32_t i = 0; i < JOSH_NET_MAX_DEVICES; ++i) {
        net_device_config_t config = {0};
        config.mtu = 1500;
        config.context = &captures[i];
        config.transmit = capture_transmit;
        assert(net_register_device(&config, &id) == NET_OK);
    }
    net_device_config_t overflow = {0};
    overflow.mtu = 1500;
    overflow.transmit = capture_transmit;
    assert(net_register_device(&overflow, &id) == NET_NO_CAPACITY);
}

int main(void) {
    test_device_send();
    test_loopback();
    test_capacity_and_arguments();
    for (int status = NET_OK; status <= NET_DRIVER_ERROR; ++status) {
        assert(net_status_string((net_status_t)status) != NULL);
    }
    assert(net_status_string((net_status_t)999) != NULL);
    puts("network device tests passed");
    return 0;
}
