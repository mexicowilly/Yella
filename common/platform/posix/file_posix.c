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
#include "common/text_util.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

static bool yella_do_stat(const char* const name, struct stat* info)
{
    int err;

    if (stat(name, info) != 0)
    {
        err = errno;
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "Could not get information about %s: %s",
                       name,
                       strerror(err));
        return false;
    }
    return true;
}

static const char* yella_last_not(const char* const str, char c)
{
    const char* result;
    size_t len;

    len = strlen(str);
    if (len == 0)
        return NULL;
    result = str + len - 1;
    while (result >= str && *result == c)
        result --;
    return (result < str) ? NULL : result;
}

char* yella_base_name(const char* const path)
{
    size_t len;
    size_t span;
    const char* end;
    const char* slash;
    char* result;

    len = strlen(path);
    if (len == 0)
        return yella_text_dup(".");
    span = strspn(path, "/");
    if (span == len)
        return yella_text_dup("/");
    end = yella_last_not(path, '/');
    slash = end;
    while (*slash != '/' && slash >= path)
        slash--;
    if (slash < path)
        return yella_text_dup(path);
    len = end - slash;
    result = malloc(len + 1);
    strncpy(result, slash + 1, len);
    result[len] = 0;
    return result;
}

yella_rc yella_create_directory(const char* const name)
{
    yella_rc result;
    int err;

    if(mkdir(name, 0755) == 0)
    {
        result = YELLA_NO_ERROR;
    }
    else
    {
        err = errno;
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "Could not create directory %s: %s",
                       name,
                       strerror(err));
        if (err == EACCES)
            result = YELLA_NO_PERMISSION;
        else if (err == EEXIST)
            result = YELLA_ALREADY_EXISTS;
        else
            result = YELLA_FILE_SYSTEM_ERROR;
    }
    return result;
}

char* yella_dir_name(const char* const path)
{
    size_t len;
    char* result;
    const char* end;
    const char* slash;
    size_t span;

    len = strlen(path);
    if (len == 0)
        return yella_text_dup(".");
    span = strspn(path, "/");
    if (span == len)
        return yella_text_dup("/");
    end = yella_last_not(path, '/');
    slash = end;
    while (*slash != '/' && slash >= path)
        slash--;
    if (slash < path)
        return yella_text_dup(".");
    end = slash;
    while (*end == '/' && end >= path)
        end--;
    if (end < path)
        return yella_text_dup("/");
    len = end - path + 1;
    result = malloc(len + 1);
    strncpy(result, path, len);
    result[len] = 0;
    return result;
}

yella_rc yella_ensure_dir_exists(const char* const name)
{
    struct stat info;
    char rp[PATH_MAX + 1];
    int err;
    char** elems;
    size_t idx;
    size_t capacity;

    if (realpath(name, rp) == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "Could not resolve real path of %s: %s",
                       name,
                       strerror(err));
        return YELLA_FILE_SYSTEM_ERROR;
    }
    capacity = 10;
    elems = malloc(capacity * sizeof(char*));
    idx = 0;

    if (!yella_file_exists(name))
        return YELLA_DOES_NOT_EXIST;
    if (!yella_do_stat(name, &info))
        return YELLA_FILE_SYSTEM_ERROR;
    if (S_ISDIR(info.st_mode))
        return YELLA_NO_ERROR;
}

bool yella_file_exists(const char* const name)
{
    return access(name, F_OK) == 0;
}

uint64_t yella_file_size(const char* const name)
{
    struct stat info;

    return yella_do_stat(name, &info) ? info.st_size : UINT64_MAX;
}
