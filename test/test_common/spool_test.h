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

#if !defined(YELLA_TEST_COMMON_SPOOL_TEST_H__)
#define YELLA_TEST_COMMON_SPOOL_TEST_H__

#include "common/ptr_vector.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct read_burst
{
    size_t count;
    double* messages_per_second;
} read_burst;

typedef struct read_pause
{
    size_t milliseconds;
    bool disconnect;
} read_pause;

typedef struct read_step
{
    read_burst* burst;
    read_pause* pause;
} read_step;

read_step* create_read_burst(size_t count);
read_step* create_read_pause(size_t milliseconds, bool disconnect);
yella_ptr_vector* create_read_step_vector(void);
uint8_t* pack_spool_test(const yella_ptr_vector* steps, size_t* size);
void set_read_burst_messages_per_second(read_burst* burst, double messages_per_second);
yella_ptr_vector* unpack_spool_test(const uint8_t const* msg);

#endif