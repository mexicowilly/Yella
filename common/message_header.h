/*
 * Copyright 2016 Will Mason
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if !defined(MESSAGE_HEADER_H__)
#define MESSAGE_HEADER_H__

#include "export.h"
#include "common/sds.h"
#include <chucho/logger.h>
#include <stdint.h>
#include <time.h>

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
    sds identifier;
    yella_group_disposition disposition;
} yella_group;

typedef struct yella_message_header
{
    time_t time;
    sds sender;
    sds recipient;
    sds type;
    yella_compression cmp;
    yella_sequence seq;
    yella_group* grp;
} yella_message_header;

YELLA_EXPORT yella_message_header* yella_create_mhdr(void);
YELLA_EXPORT void yella_destroy_mhdr(yella_message_header* mhdr);
YELLA_EXPORT void yella_log_mhdr(const yella_message_header* const mhdr, chucho_logger_t* lgr);
YELLA_EXPORT uint8_t* yella_pack_mhdr(const yella_message_header* const mhdr, size_t* size);
YELLA_EXPORT yella_message_header* yella_unpack_mhdr(const uint8_t* const bytes);

#endif