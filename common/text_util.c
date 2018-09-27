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

/*
#include "text_util.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

char* yella_sprintf(const char* const fmt, ...)
{
    va_list args;
    va_list args2;
    int req;
    char* result;

    va_start(args, fmt);
    va_copy(args2, args);
    req = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);
    result = malloc(req);
    vsnprintf(result, req, fmt, args2);
    va_end(args2);
    return result;
}

char* yella_text_dup(const char* const t)
{
    size_t len = strlen(t) + 1;
    char* result = malloc(len);
    if (result != NULL)
        strcpy(result, t);
    return result;
}

uintmax_t yella_text_to_int(const char* const t)
{
    return strtoull(t, NULL, 10);
}
*/