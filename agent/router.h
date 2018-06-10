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

#if !defined(ROUTER_H__)
#define ROUTER_H__

#include "yella_uuid.h"
#include "msg_part.h"
#include "common/return_code.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum
{
    YELLA_ROUTER_CONNECTION_PENDING,
    YELLA_ROUTER_CONNECTED,
    YELLA_ROUTER_DISCONNECTED,
    YELLA_ROUTER_SOCKET_CLOSED
} yella_router_state;

typedef struct yella_router yella_router;
typedef struct yella_sender yella_sender;

typedef void (*yella_router_state_callback)(yella_router_state, void*);
typedef void (*yella_router_message_received_callback)(const yella_msg_part* header,
                                                       const yella_msg_part* body,
                                                       void* caller_data);

yella_router* yella_create_router(yella_uuid* id);
void yella_destroy_router(yella_router* rtr);
yella_router_state yella_router_get_state(yella_router* rtr);
void yella_set_router_state_callback(yella_router* rtr,
                                     yella_router_state_callback cb,
                                     void* data);
void yella_set_router_message_received_callback(yella_router* rtr,
                                                yella_router_message_received_callback cb,
                                                void* data);

yella_sender* yella_create_sender(yella_router* rtr);
void yella_destroy_sender(yella_sender* sndr);
/**
 * @note This function takes ownership of the data, but not of the msgs
 * array itself.
 */
bool yella_send(yella_sender* sndr, yella_msg_part* msgs, size_t count);

#endif
