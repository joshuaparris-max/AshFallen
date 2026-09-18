#ifndef JOSHOS_PARTITION_H
#define JOSHOS_PARTITION_H

#include <stdint.h>
#include "block.h"

typedef enum {
    JOSH_PARTITION_OK = 0,
    JOSH_PARTITION_NO_TABLE,
    JOSH_PARTITION_NO_PARTITIONS,
    JOSH_PARTITION_IO_ERROR,
    JOSH_PARTITION_CORRUPT,
    JOSH_PARTITION_UNSUPPORTED
} josh_partition_status_t;

typedef enum {
    JOSH_PARTITION_SCHEME_NONE = 0,
    JOSH_PARTITION_SCHEME_MBR,
    JOSH_PARTITION_SCHEME_GPT
} josh_partition_scheme_t;

typedef struct {
    josh_partition_scheme_t scheme;
    uint32_t index;
    uint64_t first_lba;
    uint64_t sector_count;
    uint8_t bootable;
    uint8_t mbr_type;
    uint8_t gpt_type_guid[16];
    uint8_t gpt_unique_guid[16];
} josh_partition_t;

typedef struct {
    josh_partition_scheme_t scheme;
    uint32_t partition_count;
} josh_partition_summary_t;

typedef void (*josh_partition_visitor_fn)(
    const josh_partition_t *partition,
    void *context
);

josh_partition_status_t josh_partition_scan(
    const josh_block_device_t *device,
    josh_partition_summary_t *summary,
    josh_partition_visitor_fn visitor,
    void *visitor_context
);

const char *josh_partition_status_string(josh_partition_status_t status);

#endif
