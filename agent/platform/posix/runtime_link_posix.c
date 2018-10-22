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

#include "common/text_util.h"
#include <chucho/log.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdlib.h>

void close_shared_object(void* handle)
{
    dlclose(handle);
}

void* open_shared_object(const UChar* const file_name, chucho_logger_t* lgr)
{
    void* handle;
    char* utf8;

    utf8 = yella_to_utf8(file_name);
    handle = dlopen(utf8, RTLD_LAZY);
    if(handle == NULL)
    {
        CHUCHO_C_ERROR_L(lgr,
                         "The shared object %s could not be loaded: %s",
                         utf8,
                         dlerror());
    }
    free(utf8);
    return handle;
}

void* shared_object_symbol(void* handle, const UChar* const name, chucho_logger_t* lgr)
{
    char* utf8;
    void* sym;

    utf8 = yella_to_utf8(name);
    sym = dlsym(handle, utf8);
    if (sym == NULL)
    {
        CHUCHO_C_ERROR_L(lgr,
                         "The symbol %s could not be found: %s",
                         utf8,
                         dlerror());
    }
    free(utf8);
    return sym;
}
