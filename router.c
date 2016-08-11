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

#include "router.h"
#include "common/settings.h"
#include "common/log.h"
#include "common/thread.h"
#include <zmq.h>
#include <stdlib.h>

typedef struct yella_router
{
    void* context;
    void* socket;
    yella_mutex* mtx;
} yella_router;

static void yella_zmq_free(void* data, void* part_data)
{
    free(data);
    free(part_data);
}

yella_router* yella_create_router(void)
{
    int rc;
    const char* ep;
    yella_router* rtr;

    rtr = malloc(sizeof(yella_router));
    rtr->mtx = NULL;
    rtr->context = zmq_ctx_new();
    rtr->socket = zmq_socket(rtr->context, ZMQ_DEALER);
    if (rtr->socket == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "No sockets could be created");
        goto err_out;
    }
    ep = yella_settings_get_text("router");
    if (ep == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "No router has been defined in settings");
        goto err_out;
    }
    rc = zmq_connect(rtr->socket, yella_settings_get_text("router"));
    if (rc != 0)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "Could not connect to %s: %s",
                       ep,
                       zmq_strerror(rc));

        goto err_out;
    }
    rtr->mtx = yella_create_mutex();
    return rtr;
err_out:
    yella_destroy_router(rtr);
    return NULL;

}

void yella_destroy_router(yella_router* rtr)
{
    if (rtr->mtx != NULL)
        yella_destroy_mutex(rtr->mtx);
    if (rtr->socket != NULL)
        zmq_close(rtr->socket);
    if (rtr->context != NULL)
        zmq_ctx_destroy(rtr->context);
    free(rtr);
}

void yella_router_send(yella_router* rtr, yella_msg_part* msgs, size_t count)
{
    zmq_msg_t msg;
    size_t i;
    int rc;

    for (i = 0; i < count; i++)
    {
        zmq_msg_init_data(&msg, msgs[i].msg, msgs[i].size, yella_zmq_free, &msgs[i]);
        yella_lock_mutex(rtr->mtx);
        rc = zmq_msg_send(&msg, rtr->socket, (i == count - 1) ? 0 : ZMQ_SNDMORE);
        yella_unlock_mutex(rtr->mtx);
        if (rc != msgs[i].size)
        {
            CHUCHO_C_ERROR(yella_logger("yella"),
                           "Could not send message: %s",
                           zmq_strerror(rc));
            zmq_msg_close(&msg);
            break;
        }
    }
    /* Release unsent message data */
    for ( ; i < count; i++)
    {
        free(msgs[i].msg);
        free(&msgs[i]);
    }
    free(msgs);
}
