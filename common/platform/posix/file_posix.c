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

#include "common/file.h"
#include "common/log.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

bool yella_file_exists(const char* const name)
{
    return access(name, F_OK) == 0;
}

uint64_t yella_file_size(const char* const name)
{
    struct stat info;
    int err;

    if (stat(name, &info) != 0)
    {
        err = errno;
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "Could not get the size of %s: %s",
                       name,
                       strerror(err));
        return UINT64_MAX;
    }
    return info.st_size;
}
