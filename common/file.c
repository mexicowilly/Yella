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
#include "common/text_util.h"
#include <chucho/log.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

yella_rc yella_file_contents(const UChar* const name, uint8_t** contents)
{
    FILE* f;
    size_t size = 0;
    int err;
    size_t num_read;
    char* utf8;

    if (!yella_file_exists(name))
        return YELLA_DOES_NOT_EXIST;
    yella_file_size(name, &size);
    utf8 = yella_to_utf8(name);
    f = fopen(utf8, "rb");
    if (f == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.common",
                       "Could not open %s for reading: %s",
                       utf8,
                       strerror(err));
        free(utf8);
        return YELLA_FILE_SYSTEM_ERROR;
    }
    *contents = malloc(size);
    num_read = fread(*contents, 1, size, f);
    fclose(f);
    if (num_read != size)
    {
        CHUCHO_C_ERROR("yella.common",
                       "Could not read %s",
                       utf8);
        free(utf8);
        free(*contents);
        return YELLA_FILE_SYSTEM_ERROR;
    }
    free(utf8);
    return YELLA_NO_ERROR;
}
