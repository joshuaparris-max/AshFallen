#ifndef JOSHOS_PARTITION_H
#define JOSHOS_PARTITION_H

#include "block.h"
#include <stdint.h>

#define PARTITION_MAX 16u

typedef enum {
    PARTITION_SCHEME_NONE = 0,
    PARTITION_SCHEME_MBR,
    PARTITION_SCHEME_GPT
} partition_scheme_t;

typedef struct {
    uint64_t first_lba;
    uint64_t sector_count;
    uint8_t mbr_type;
    uint8_t bootable;
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
} partition_t;

typedef struct {
    partition_scheme_t scheme;
    uint32_t count;
    partition_t entries[PARTITION_MAX];
} partition_table_t;

typedef enum {
    PARTITION_OK = 0,
    PARTITION_BAD_ARGUMENT,
    PARTITION_IO_ERROR,
    PARTITION_BAD_SIGNATURE,
    PARTITION_BAD_GPT,
    PARTITION_TOO_MANY
} partition_status_t;

partition_status_t partition_scan(const block_device_t *device, partition_table_t *table);
const char *partition_status_string(partition_status_t status);

#endif
