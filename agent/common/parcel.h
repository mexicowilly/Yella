#ifndef YELLA_PARCEL_H__
#define YELLA_PARCEL_H__

#include "export.h"
#include "common/uds.h"
#include <stdint.h>
#include <unicode/ucal.h>

typedef enum
{
    YELLA_COMPRESSION_NONE,
    YELLA_COMPRESSION_LZ4
} yella_compression;

typedef enum
{
    YELLA_GROUP_DISPOSITION_MORE,
    YELLA_GROUP_DISPOSITION_END
} yella_group_disposition;

typedef struct yella_sequence
{
    uint32_t major;
    uint32_t minor;
} yella_sequence;

typedef struct yella_group
{
    uds identifier;
    yella_group_disposition disposition;
} yella_group;

typedef struct yella_parcel
{
    UDate time;
    uds sender;
    uds recipient;
    uds type;
    yella_compression cmp;
    yella_sequence seq;
    yella_group* grp;
    uint8_t* payload;
    size_t payload_size;
} yella_parcel;

struct chucho_logger_t;

YELLA_EXPORT yella_parcel* yella_create_parcel(const UChar* const recipient, const UChar* const type);
YELLA_EXPORT void yella_destroy_parcel(yella_parcel* pcl);
YELLA_EXPORT void yella_log_parcel(const yella_parcel* const pcl, struct chucho_logger_t* lgr);
YELLA_EXPORT uint8_t* yella_pack_parcel(const yella_parcel* const pcl, size_t* size);
YELLA_EXPORT yella_parcel* yella_unpack_parcel(const uint8_t* const bytes);

#endif
