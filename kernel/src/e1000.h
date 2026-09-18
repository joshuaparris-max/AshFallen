#ifndef JOSHOS_E1000_H
#define JOSHOS_E1000_H

#include <stdint.h>

typedef enum {
    E1000_OK = 0,
    E1000_NOT_FOUND,
    E1000_PCI_ERROR,
    E1000_MMIO_ERROR,
    E1000_RESET_TIMEOUT,
    E1000_NO_MEMORY,
    E1000_BAD_MAC,
    E1000_NET_REGISTER_ERROR,
    E1000_TX_TIMEOUT
} e1000_status_t;

e1000_status_t e1000_init(uint32_t *net_device_id_out);
void e1000_poll(void);
int e1000_link_up(void);
const uint8_t *e1000_mac_address(void);
const char *e1000_status_string(e1000_status_t status);

#endif
