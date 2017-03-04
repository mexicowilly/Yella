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
#include "test/test_common/spool_test.h"
#include "test/test_common/spool_test_result.h"
#include "common/thread.h"
#include "common/time_util.h"
#include "common/text_util.h"
#include <zmq.h>
#include <stdlib.h>

void* spool_test(void* ctx, void* sock, const msg_pair const* in_mp)
{
    yella_ptr_vector* steps;
    unsigned i;
    const read_step* step;
    uint32_t j;
    msg_pair* read_mp;
    double msgs_per_sec;
    uintmax_t start_millis;
    spool_test_result res;
    msg_pair* out_mp;

    steps = unpack_spool_test(in_mp->body);
    start_millis = yella_millis_since_epoch();
    res.message_count = 0;
    for (i = 0; i < yella_ptr_vector_size(steps); i++)
    {
        step = yella_ptr_vector_at(steps, i);
        if (step->burst != NULL)
        {
            msgs_per_sec = 0.0;
            for (j = 0; j < step->burst->count; j++)
            {
                read_mp = read_message(sock, 1000);
                if (read_mp == NULL)
                {

                }
                destroy_msg_pair(read_mp);
                res.message_count++;
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
    res.elapsed_milliseconds = yella_millis_since_epoch() - start_millis;
    out_mp = malloc(sizeof(msg_pair));
    out_mp->hdr = yella_create_mhdr();
    out_mp->hdr->sender = yella_text_dup("spool_test");
    out_mp->hdr->recipient = yella_text_dup(in_mp->hdr->sender);
    out_mp->hdr->type = yella_text_dup("spool_test_result");
    out_mp->body = pack_spool_test_result(&res, &out_mp->body_size);
    send_message(sock, out_mp);
    destroy_msg_pair(out_mp);
    return sock;
}