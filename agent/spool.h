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

#include "saved_state.h"
#include "msg_part.h"
#include <stdbool.h>

typedef struct yella_spool yella_spool;

yella_spool* yella_create_spool(const yella_saved_state* state);
void yella_destroy_spool(yella_spool* sp);
yella_msg_part* yella_spool_pop(yella_spool* sp, size_t milliseconds_to_wait, size_t* count);
bool yella_spool_push(yella_spool* sp, yella_msg_part* msgs, size_t count);

#endif
