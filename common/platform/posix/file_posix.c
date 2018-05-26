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
#include "common/ptr_vector.h"
#include <chucho/log.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <dirent.h>

const char* YELLA_DIR_SEP = "/";

struct yella_directory_iterator
{
    DIR* dir;
    struct dirent* entry;
    char fqn[PATH_MAX + 1];
    size_t dir_name_len;
};

static bool yella_do_stat(const char* const name, struct stat* info)
{
    int err;

    if (stat(name, info) != 0)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.common",
                       "Could not get information about %s: %s",
                       name,
                       strerror(err));
        return false;
    }
    return true;
}

static yella_ptr_vector* yella_get_dirs(const char* const fqpath)
{
    char* cur;
    yella_ptr_vector* vec;

    vec = yella_create_ptr_vector();
    cur = yella_dir_name(fqpath);
    while (strcmp(cur, "/") != 0)
    {
        yella_push_front_ptr_vector(vec, cur);
        cur = yella_dir_name(cur);
    }
    free(cur);
    return vec;
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
        CHUCHO_C_ERROR("yella.common",
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

yella_directory_iterator* yella_create_directory_iterator(const char* const dir)
{
    yella_directory_iterator* result;
    int err;
    size_t len;

    result = malloc(sizeof(yella_directory_iterator));
    result->dir = opendir(dir);
    if (result->dir == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.common",
                       "Could not open directory %s for reading: %s",
                       dir,
                       strerror(err));
        free(result);
        return NULL;
    }
    result->entry = malloc(sizeof(struct dirent) + NAME_MAX + 1);
    strcpy(result->fqn, dir);
    len = strlen(result->fqn);
    if (result->fqn[len - 1] != YELLA_DIR_SEP[0])
        strcat(result->fqn, YELLA_DIR_SEP);
    result->dir_name_len = strlen(result->fqn);
    return result;
}

void yella_destroy_directory_iterator(yella_directory_iterator* itor)
{
    free(itor->entry);
    closedir(itor->dir);
    free(itor);
}

const char* yella_directory_iterator_next(yella_directory_iterator* itor)
{
    struct dirent* found;

    do
    {
        found = readdir(itor->dir);
    } while (found != NULL &&
             (strcmp(found->d_name, ".") == 0 || strcmp(found->d_name, "..") == 0));
    if (found == NULL)
        return NULL;
    itor->fqn[itor->dir_name_len] = 0;
    strcat(itor->fqn, found->d_name);
    return itor->fqn;
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
    char rp[PATH_MAX + 1];
    int err;
    yella_ptr_vector* dirs;
    size_t i;
    yella_rc rc;
    const char* cur;

    if (realpath(name, rp) == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.common",
                       "Could not resolve real path of %s: %s",
                       name,
                       strerror(err));
        return YELLA_FILE_SYSTEM_ERROR;
    }
    dirs = yella_get_dirs(rp);
    yella_push_back_ptr_vector(dirs, yella_text_dup(rp));
    for (i = 0; i < yella_ptr_vector_size(dirs); i++)
    {
        cur = yella_ptr_vector_at(dirs, i);
        if (!yella_file_exists(cur))
        {
            rc = yella_create_directory(cur);
            if (rc != YELLA_NO_ERROR)
                break;
        }
    }
    yella_destroy_ptr_vector(dirs);
    return rc;
}

bool yella_file_exists(const char* const name)
{
    return access(name, F_OK) == 0;
}

yella_rc yella_file_size(const char* const name, size_t* sz)
{
    struct stat info;
    int err;
    yella_rc yrc;

    if (stat(name, &info) == 0)
    {
        *sz = info.st_size;
        yrc = YELLA_NO_ERROR;
    }
    else
    {
        err = errno;
        if (err == EACCES)
            yrc = YELLA_NO_PERMISSION;
        else if (err == ENOENT)
            yrc = YELLA_DOES_NOT_EXIST;
        else
            yrc = YELLA_FILE_SYSTEM_ERROR;
        CHUCHO_C_ERROR("yella.common",
                       "Could not get information about %s: %s",
                       name,
                       strerror(err));
    }
    return yrc;
}
