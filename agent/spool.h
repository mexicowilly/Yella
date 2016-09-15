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

#if !defined(SPOOL_H__)
#define SPOOL_H__

#include "agent/router.h"
#include "agent/saved_state.h"

typedef struct yella_spool yella_spool;

typedef void (*yella_spool_size_notification)(double percent_full, void* data);

yella_spool* yella_create_spool(const yella_saved_state* state, yella_router* rtr);
void yella_destroy_spool(yella_spool* sp);
void yella_set_spool_size_notification(yella_spool* sp, yella_spool_size_notification sn, void* data);
bool yella_write_spool(yella_spool* sp, yella_msg_part* msgs, size_t count);

#endif
