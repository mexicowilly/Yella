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
    char* identifier;
    yella_group_disposition disposition;
} yella_group;

typedef struct yella_message_header yella_message_header;

uint8_t* yella_pack_mhdr(const yella_message_header const* mhdr, size_t* size);
yella_message_header* yella_unpack_mhdr(const uint8_t const* bytes);
void yella_destroy_mhdr(yella_message_header* mhdr);

const yella_compression yella_mhdr_get_compression(const yella_message_header const* mhdr);
const yella_group* yella_mhdr_get_group(const yella_message_header const* mhdr);
const char* yella_mhdr_get_message_type(const yella_message_header const* mhdr);
const char* yella_mhdr_get_recipient(const yella_message_header const* mhdr);
const char* yella_mhdr_get_sender(const yella_message_header const* mhdr);
const yella_sequence* yella_mhdr_get_sequence(const yella_message_header const* mhdr);
time_t yella_mhdr_get_time(const yella_message_header const* mhdr);

#endif