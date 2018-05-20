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

#include <chucho/log.h>
#include <dlfcn.h>
#include <stddef.h>

void close_shared_object(void* handle)
{
    dlclose(handle);
}

void* open_shared_object(const char* const file_name)
{
    void* handle;

    handle = dlopen(file_name, RTLD_LAZY);
    if(handle == NULL)
    {
        CHUCHO_C_ERROR("yella.agent",
                       "The shared object %s could not be loaded: %s",
                       file_name,
                       dlerror());
    }
    return handle;
}

void* shared_object_symbol(void* handle, const char* const name)
{
    void* sym = dlsym(handle, name);
    if (sym == NULL)
    {
        CHUCHO_C_ERROR("yella.agent",
                       "The symbol %s could not be found: %s",
                       name,
                       dlerror());
    }
    return sym;
}
