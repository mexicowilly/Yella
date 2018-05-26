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

#include "saved_state.h"
#include "msg_part.h"
#include "common/return_code.h"
#include <stdbool.h>

typedef struct yella_spool yella_spool;

typedef struct yella_spool_stats
{
    size_t partition_size;
    size_t max_size;
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
} yella_spool_stats;

yella_spool* yella_create_spool(const yella_saved_state* state);
void yella_destroy_spool(yella_spool* sp);
yella_spool_stats yella_pool_get_stats(yella_spool* sp);
yella_rc yella_spool_pop(yella_spool* sp,
                         size_t milliseconds_to_wait,
                         yella_msg_part** parts,
                         size_t* count);
yella_rc yella_spool_push(yella_spool* sp, yella_msg_part* msgs, size_t count);

#endif
