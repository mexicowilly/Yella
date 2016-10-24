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

#include "fake_router.h"
#include "spool_test.h"
#include "common/ptr_vector.h"
#include "common/thread.h"
#include "spool_test_reader.h"
#include <stdbool.h>
#include <zmq.h>

typedef struct read_burst
{
    size_t count;
    double messages_per_second;
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

void destroy_read_step(void* p, void* udata)
{
    read_step* rs;

    rs = (read_step*)p;
    free(rs->burst);
    free(rs->pause);
    free(rs);
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

    vec = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(vec, destroy_read_step, NULL);
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
            cur->burst->messages_per_second = yella_fb_read_burst_messages_per_second(burst);
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

void* spool_test(void* ctx, void* sock, const uint8_t const* msg)
{
    yella_ptr_vector* steps;
    unsigned i;
    read_step* step;

    steps = unpack_spool_test(msg);
    for (i = 0; i < yella_ptr_vector_size(steps); i++)
    {
        step = yella_ptr_vector_at(steps, i);
        if (step->burst != NULL)
        {
        }
        else if (step->pause != NULL)
        {
            if (step->pause->disconnect)
                zmq_close(sock);
            yella_sleep_this_thread(step->pause->milliseconds);
            if (step->pause->disconnect)
                sock = create_socket(ctx);
        }
    }
    yella_destroy_ptr_vector(steps);
    return sock;
}