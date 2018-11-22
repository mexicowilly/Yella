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

#if !defined(SPOOL_H__)
#define SPOOL_H__

#include "common/return_code.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct spool spool;

typedef struct spool_stats
{
    size_t max_partition_size;
    size_t max_partitions;
    size_t current_size;
    size_t largest_size;
    size_t files_created;
    size_t files_destroyed;
    size_t bytes_culled;
    size_t events_read;
    size_t events_written;
    size_t smallest_event_size;
    size_t largest_event_size;
    size_t average_event_size;
    size_t cull_events;
} spool_stats;

typedef struct message_part
{
    uint8_t* data;
    size_t size;
} message_part;

spool* create_spool(void);
void destroy_spool(spool* sp);
bool spool_empty_of_messages(spool * sp);
spool_stats spool_get_stats(spool* sp);
yella_rc spool_pop(spool* sp,
                   size_t milliseconds_to_wait,
                   message_part** parts,
                   size_t* count);
yella_rc spool_push(spool* sp, const message_part* msgs, size_t count);

#endif
