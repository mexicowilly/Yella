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

#if !defined(SAVED_STATE_H__)
#define SAVED_STATE_H__

#include "yella_uuid.h"
#include "mac_addresses.h"
#include "common/return_code.h"
#include <stdint.h>

typedef struct yella_saved_state
{
    uint32_t boot_count;
    yella_uuid* id;
    yella_mac_addresses* mac_addresses;
} yella_saved_state;

void yella_destroy_saved_state(yella_saved_state* ss);
yella_saved_state* yella_load_saved_state(void);
yella_rc yella_save_saved_state(yella_saved_state* ss);

#endif
