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
#include <zmq.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    void* zmctx;
    void* sock;
    msg_pair* mp;

    zmctx = zmq_ctx_new();
    sock = create_socket(zmctx);
    while (true)
    {
        mp = read_message(sock, 10 * 1000);
        if (strcmp(mp->hdr->type, "spool_test") == 0)
            sock = spool_test(zmctx, sock, mp);
        destroy_msg_pair(mp);
    }
    zmq_close(sock);
    zmq_ctx_term(zmctx);
    return EXIT_SUCCESS;
}