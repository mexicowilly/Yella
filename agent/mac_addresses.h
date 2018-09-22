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

#include <stdlib.h>
#include <stdint.h>
#include <chucho/logger.h>

#if !defined(MAC_ADDRESSES_H__)
#define MAC_ADDRESSES_H__

typedef struct yella_mac_address
{
    uint8_t addr[6];
    char text[18];
} yella_mac_address;

typedef struct yella_mac_addresses
{
    yella_mac_address* addrs;
    size_t count;
} yella_mac_addresses;

void yella_destroy_mac_addresses(yella_mac_addresses* addrs);
yella_mac_addresses* yella_get_mac_addresses(chucho_logger_t* lgr);

#endif
