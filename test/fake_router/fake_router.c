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
#include <zmq.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void* create_socket(void* ctx)
{
    void* sock;
    int rc;

    sock = zmq_socket(ctx, ZMQ_ROUTER);
    assert(sock != NULL);
    rc = zmq_bind(sock, "tcp://*:19567");
    assert(rc == 0);
    return sock;
}

msg_pair read_message(void* sock)
{
    zmq_msg_t id;
    zmq_msg_t delim;
    zmq_msg_t hdr;
    zmq_msg_t body;
    int rc;
    msg_pair result;

    result.hdr = NULL;
    result.body = NULL;
    zmq_msg_init(&id);
    rc = zmq_msg_recv(&id, sock, 0);
    assert(rc != -1);
    assert(zmq_msg_more(&id) != 0);
    zmq_msg_close(&id);
    zmq_msg_init(&delim);
    rc = zmq_msg_recv(&delim, sock, 0);
    assert(rc != -1);
    assert(zmq_msg_more(&delim) != 0);
    assert(zmq_msg_size(&delim) == 0);
    zmq_msg_close(&delim);
    zmq_msg_init(&hdr);
    rc = zmq_msg_recv(&hdr, sock, 0);
    assert(rc != -1);
    assert(zmq_msg_more(&hdr) != 0);
    assert(zmq_msg_size(&hdr) > 0);
    result.hdr = yella_unpack_mhdr(zmq_msg_data(&hdr));
    zmq_msg_close(&hdr);
    zmq_msg_init(&body);
    rc = zmq_msg_recv(&body, sock, 0);
    assert(rc != -1);
    assert(zmq_msg_more(&body) == 0);
    assert(zmq_msg_size(&body) > 0);
    result.body = malloc(zmq_msg_size(&body));
    memcpy(result.body, zmq_msg_data(&body), zmq_msg_size(&body));
    zmq_msg_close(&body);
    return result;
}