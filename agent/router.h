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
#include "common/message_part.h"
#include "common/return_code.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum
{
    ROUTER_CONNECTION_PENDING,
    ROUTER_CONNECTED,
    ROUTER_DISCONNECTED,
    ROUTER_SOCKET_CLOSED
} router_state;

typedef struct router router;
typedef struct sender sender;

typedef void (*router_state_callback)(router_state, void*);
typedef void (*router_message_received_callback)(const yella_message_part* const header,
                                                 const yella_message_part* const body,
                                                 void* caller_data);

router* create_router(yella_uuid* id);
void destroy_router(router* rtr);
router_state get_router_state(router* rtr);
void set_router_state_callback(router* rtr,
                               router_state_callback cb,
                               void* data);
void set_router_message_received_callback(router* rtr,
                                          router_message_received_callback cb,
                                          void* data);

sender* create_sender(router* rtr);
void destroy_sender(sender* sndr);
/**
 * @note This function takes ownership of the data, but not of the msgs
 * array itself.
 */
bool send_router_message(sender* sndr, yella_message_part* msgs, size_t count);
bool send_transient_router_message(sender* sndr, yella_message_part* msgs, size_t count);

#endif
