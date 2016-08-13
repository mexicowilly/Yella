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
#include <stdint.h>
#include <stddef.h>

typedef enum
{
    YELLA_ROUTER_CONNECTION_PENDING,
    YELLA_ROUTER_CONNECTED,
    YELLA_ROUTER_DISCONNECTED,
    YELLA_ROUTER_SOCKET_CLOSED
} yella_router_state;

typedef struct yella_router yella_router;

typedef struct yella_msg_part
{
    uint8_t* msg;
    size_t size;
} yella_msg_part;

yella_router* yella_create_router(yella_uuid* id);
void yella_destroy_router(yella_router* rtr);
/**
 * @note This function takes ownership of msgs
 */
void yella_router_send(yella_router* rtr, yella_msg_part* msgs, size_t count);
yella_router_state yella_router_get_state(void);

#endif
