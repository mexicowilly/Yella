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

#include "spool_test.h"
#include "common/ptr_vector.h"
#include "common/thread.h"
#include "spool_test_builder.h"
#include <assert.h>

read_step* create_read_burst(size_t count)
{
    read_step* result;

    result = malloc(sizeof(read_step));
    result->burst = malloc(sizeof(read_burst));
    result->pause = NULL;
    result->burst->count = count;
    result->burst->messages_per_second = NULL;
    return result;
}

read_step* create_read_pause(size_t milliseconds, bool disconnect)
{
    read_step* result;

    result = malloc(sizeof(read_step));
    result->pause = malloc(sizeof(read_pause));
    result->burst = NULL;
    result->pause->milliseconds = milliseconds;
    result->pause->disconnect = disconnect;
    return result;
}

static void destroy_read_step(void* p, void* udata)
{
    read_step* rs;

    rs = (read_step*)p;
    free(rs->burst->messages_per_second);
    free(rs->burst);
    free(rs->pause);
    free(rs);
}

yella_ptr_vector* create_read_step_vector(void)
{
    yella_ptr_vector* vec;

    vec = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(vec, destroy_read_step, NULL);
    return vec;
}

uint8_t* pack_spool_test(const yella_ptr_vector* steps, size_t* size)
{
    flatcc_builder_t bld;
    size_t i;
    const read_step* step;
    yella_fb_read_step_impl_union_ref_t burst_or_pause;
    uint8_t* result;

    flatcc_builder_init(&bld);
    yella_fb_spool_test_start_as_root(&bld);
    yella_fb_spool_test_steps_start(&bld);
    for (i = 0; i < yella_ptr_vector_size(steps); i++)
    {
        step = yella_ptr_vector_at(steps, i);
        assert((step->burst != NULL && step->pause == NULL) || (step->burst == NULL && step->pause != NULL));
        if (step->burst != NULL)
        {
            assert(step->pause == NULL);
            yella_fb_read_burst_start(&bld);
            yella_fb_read_burst_count_add(&bld, step->burst->count);
            if (step->burst->messages_per_second != NULL)
                yella_fb_read_burst_messages_per_second_add(&bld, *step->burst->messages_per_second);
            burst_or_pause = yella_fb_read_step_impl_as_read_burst(yella_fb_read_burst_end(&bld));
        }
        else
        {
            assert(step->pause != NULL);
            yella_fb_read_pause_start(&bld);
            yella_fb_read_pause_milliseconds_add(&bld, step->pause->milliseconds);
            yella_fb_read_pause_disconnect_add(&bld, step->pause->disconnect);
            burst_or_pause = yella_fb_read_step_impl_as_read_pause(yella_fb_read_pause_end(&bld));
        }
        yella_fb_spool_test_steps_push_create(&bld, burst_or_pause);
    }
    yella_fb_spool_test_steps_end(&bld);
    yella_fb_spool_test_end_as_root(&bld);
    result = flatcc_builder_finalize_buffer(&bld, size);
    flatcc_builder_clear(&bld);
    return result;
}

void set_read_burst_messages_per_second(read_burst* burst, double messages_per_second)
{
    if (burst->messages_per_second == NULL)
        burst->messages_per_second = malloc(sizeof(double));
    *burst->messages_per_second = messages_per_second;
}

yella_ptr_vector* unpack_spool_test(const uint8_t const* msg)
{
    yella_ptr_vector* vec;
    yella_fb_read_step_vec_t steps;
    yella_fb_read_step_table_t step;
    yella_fb_read_burst_table_t burst;
    yella_fb_read_pause_table_t pause;
    size_t i;
    read_step* cur;

    vec = create_read_step_vector();
    steps = yella_fb_spool_test_steps(yella_fb_spool_test_as_root(msg));
    for (i = 0; i < yella_fb_read_step_vec_len(steps); i++)
    {
        cur = malloc(sizeof(read_step));
        step = yella_fb_read_step_vec_at(steps, i);
        if (yella_fb_read_step_step_type(step) == yella_fb_read_step_impl_read_burst)
        {
            burst = (yella_fb_read_burst_table_t)yella_fb_read_step_step(step);
            cur->burst = malloc(sizeof(read_burst));
            cur->pause = NULL;
            cur->burst->count = yella_fb_read_burst_count(burst);
            if (yella_fb_read_burst_messages_per_second_is_present(burst))
            {
                cur->burst->messages_per_second = malloc(sizeof(double));
                *cur->burst->messages_per_second = yella_fb_read_burst_messages_per_second(burst);
            }
            else
            {
                cur->burst->messages_per_second = NULL;
            }
        }
        else if (yella_fb_read_step_step_type(step) == yella_fb_read_step_impl_read_pause)
        {
            pause = (yella_fb_read_pause_table_t)yella_fb_read_step_step(step);
            cur->pause = malloc(sizeof(read_pause));
            cur->burst = NULL;
            cur->pause->milliseconds = yella_fb_read_pause_milliseconds(pause);
            cur->pause->disconnect = yella_fb_read_pause_disconnect(pause);
        }
        else
        {
            free(cur);
            continue;
        }
        yella_push_back_ptr_vector(vec, cur);
    }
    return vec;
}