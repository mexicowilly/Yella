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
#include "test/test_common/spool_test.h"
#include "common/thread.h"
#include <zmq.h>

void* spool_test(void* ctx, void* sock, const uint8_t const* msg)
{
    yella_ptr_vector* steps;
    unsigned i;
    const read_step* step;
    uint32_t j;
    msg_pair* mp;
    double msgs_per_sec;

    steps = unpack_spool_test(msg);
    for (i = 0; i < yella_ptr_vector_size(steps); i++)
    {
        step = yella_ptr_vector_at(steps, i);
        if (step->burst != NULL)
        {
            msgs_per_sec = 0.0;
            for (j = 0; j < step->burst->count; j++)
            {
                mp = read_message(sock);
                destroy_msg_pair(mp);
            }
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