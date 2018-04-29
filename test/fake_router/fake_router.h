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

#if !defined(YELLA_FAKE_ROUTER_H__)
#define YELLA_FAKE_ROUTER_H__

#include "common/message_header.h"

typedef struct msg_pair
{
    yella_message_header* hdr;
    uint8_t* body;
    size_t body_size;
} msg_pair;

void* create_socket(void* ctx);
void destroy_msg_pair(msg_pair* mp);
msg_pair* read_message(void* sock, unsigned time_out_millis);
void send_message(void* sock, const msg_pair* const mp);

#endif