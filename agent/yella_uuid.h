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

#if !defined(YELLA_UUID_H__)
#define YELLA_UUID_H__

#include <stdint.h>
#include <stddef.h>

typedef struct yella_uuid yella_uuid;

yella_uuid* yella_create_uuid(void);
yella_uuid* yella_create_uuid_from_bytes(const uint8_t* bytes, size_t len);
void yella_destroy_uuid(yella_uuid* id);
const uint8_t* yella_uuid_bytes(const yella_uuid* id);
size_t yella_uuid_byte_count(const yella_uuid* id);
const char* yella_uuid_to_text(yella_uuid* id);

#endif
