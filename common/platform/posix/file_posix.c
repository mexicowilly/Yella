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
#include "common/sds_util.h"
#include <chucho/log.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>

const char* YELLA_DIR_SEP = "/";

struct yella_directory_iterator
{
    DIR* dir;
    struct dirent* entry;
    char fqn[PATH_MAX + 1];
    size_t dir_name_len;
};

void sds_ptr_destructor(void* elem, void* udata)
{
    sdsfree(elem);
}

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

    vec = yella_create_sds_ptr_vector();
    cur = yella_dir_name(fqpath);
    if (strcmp(cur, ".") == 0)
    {
        yella_push_back_ptr_vector(vec, cur);
        return vec;
    }
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

sds yella_base_name(const char* const path)
{
    size_t len;
    size_t span;
    const char* end;
    const char* slash;

    len = strlen(path);
    if (len == 0)
        return sdsnew(".");
    span = strspn(path, "/");
    if (span == len)
        return sdsnew("/");
    end = yella_last_not(path, '/');
    slash = end;
    while (*slash != '/' && slash >= path)
        slash--;
    if (slash < path)
        return sdsnew(path);
    return sdsnewlen(slash + 1, end - slash);
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

sds yella_dir_name(const char* const path)
{
    size_t len;
    const char* end;
    const char* slash;
    size_t span;

    len = strlen(path);
    if (len == 0)
        return sdsnew(".");
    span = strspn(path, "/");
    if (span == len)
        return sdsnew("/");
    end = yella_last_not(path, '/');
    slash = end;
    while (*slash != '/' && slash >= path)
        slash--;
    if (slash < path)
        return sdsnew(".");
    end = slash;
    while (*end == '/' && end >= path)
        end--;
    if (end < path)
        return sdsnew("/");
    return sdsnewlen(path, end - path + 1);
}

yella_rc yella_ensure_dir_exists(const char* const name)
{
    yella_ptr_vector* dirs;
    size_t i;
    yella_rc rc = YELLA_NO_ERROR;
    const char* cur;

    if (name == NULL || name[0] == 0)
        return YELLA_LOGIC_ERROR;
    dirs = yella_get_dirs(name);
    if (strcmp(yella_ptr_vector_at(dirs, 0), ".") == 0)
        yella_pop_front_ptr_vector(dirs);
    yella_push_back_ptr_vector(dirs, sdsnew(name));
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

char* yella_getcwd(void)
{
    return getcwd(NULL, 0);
}

static yella_rc remove_all_impl(const char* const name)
{
    yella_directory_iterator* itor = yella_create_directory_iterator(name);
    const char* cur = yella_directory_iterator_next(itor);
    struct stat st;
    yella_rc rc = YELLA_NO_ERROR;
    yella_rc cur_rc;

    while (cur != NULL)
    {
        stat(cur, &st);
        if (S_ISDIR(st.st_mode))
        {
            cur_rc = remove_all_impl(cur);
            if (cur_rc > rc)
                rc = cur_rc;
        }
        else
        {
            if (remove(cur) != 0)
            {
                CHUCHO_C_ERROR("yella.common",
                               "Unable to remove %s: %s",
                               cur,
                               strerror(errno));
                if (YELLA_FILE_SYSTEM_ERROR > rc)
                    rc = YELLA_FILE_SYSTEM_ERROR;
            }
        }
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
    if (remove(name) != 0)
    {
        CHUCHO_C_ERROR("yella.common",
                       "Unable to remove %s: %s",
                       name,
                       strerror(errno));
        if (YELLA_FILE_SYSTEM_ERROR > rc)
            rc = YELLA_FILE_SYSTEM_ERROR;
    }
    return rc;
}

yella_rc yella_remove_all(const char* const name)
{
    struct stat st;

    if (access(name, F_OK) != 0)
        return YELLA_NO_ERROR;
    if (stat(name, &st) != 0)
    {
        CHUCHO_C_ERROR("yella.common",
                       "Unable to stat %s: %s",
                       name,
                       strerror(errno));
        return YELLA_FILE_SYSTEM_ERROR;
    }
    if (S_ISDIR(st.st_mode))
    {
        return remove_all_impl(name);
    }
    if (remove(name) != 0)
    {
        CHUCHO_C_ERROR("yella.common",
                       "Unable to remove %s: %s",
                       name,
                       strerror(errno));
        return YELLA_FILE_SYSTEM_ERROR;
    }
    return YELLA_NO_ERROR;
}

