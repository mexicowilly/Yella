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
#include "common/text_util.h"
#include <chucho/log.h>
#include <uuid.h>
#include <stdlib.h>
#include <string.h>

yella_uuid* yella_create_uuid(void)
{
    yella_uuid* id;
    char utf8[37];
    UChar* utf16;

    id = malloc(sizeof(yella_uuid));
    uuid_generate(id->id);
    uuid_unparse_lower(id->id, utf8);
    utf16 = yella_from_utf8(utf8);
    u_strcpy(id->text, utf16);
    free(utf16);
    return id;
}

yella_uuid* yella_create_uuid_from_bytes(const uint8_t* bytes)
{
    yella_uuid* id;
    char utf8[37];
    UChar* utf16;

    id = malloc(sizeof(yella_uuid));
    memcpy(id->id, bytes, sizeof(id->id));
    uuid_unparse_lower(id->id, utf8);
    utf16 = yella_from_utf8(utf8);
    u_strcpy(id->text, utf16);
    free(utf16);
    return id;
}

void yella_destroy_uuid(yella_uuid* id)
{
    free(id);
}

