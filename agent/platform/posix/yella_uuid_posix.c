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

#include "agent/yella_uuid.h"
#include <chucho/log.h>
#include <uuid.h>
#include <stdlib.h>
#include <string.h>

struct yella_uuid
{
    uuid_t id;
    char* text;
};

yella_uuid* yella_create_uuid(void)
{
    yella_uuid* id;

    id = malloc(sizeof(yella_uuid));
    uuid_generate(id->id);
    id->text = NULL;
    return id;
}

yella_uuid* yella_create_uuid_from_bytes(const uint8_t* bytes, size_t len)
{
    size_t uuid_size;
    yella_uuid* id;

    uuid_size = sizeof(uuid_t);
    if (len != uuid_size)
    {
        CHUCHO_C_ERROR("yella.agent",
                       "Cannot create UUID. The given size of %zu does not match the required %zu",
                       len,
                       uuid_size);
        return NULL;
    }
    id = malloc(sizeof(yella_uuid));
    memcpy(id->id, bytes, len);
    id->text = NULL;
    return id;
}

void yella_destroy_uuid(yella_uuid* id)
{
    if (id->text != NULL)
        free(id->text);
    free(id);
}

const uint8_t* yella_uuid_bytes(const yella_uuid* id)
{
    return id->id;
}

size_t yella_uuid_byte_count(const yella_uuid* id)
{
    return sizeof(id->id);
}

const char* yella_uuid_to_text(yella_uuid* id)
{
    if (id->text == NULL)
    {
        id->text = malloc(37);
        uuid_unparse(id->id, id->text);
    }
    return id->text;
}
