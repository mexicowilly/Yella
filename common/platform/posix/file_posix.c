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
#include "common/uds_util.h"
#include "common/text_util.h"
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
#include <fcntl.h>

const UChar* YELLA_DIR_SEP = u"/";

struct yella_directory_iterator
{
    DIR* dir;
    uds dir_name;
    uds fqn;
};

static yella_rc stat_impl(const UChar* const name, struct stat* info)
{
    int err;
    yella_rc yrc;
    char* utf8;

    utf8 = yella_to_utf8(name);
    if (lstat(utf8, info) == 0)
    {
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
                       "Could not get information about '%s': %s",
                       utf8,
                       strerror(err));
    }
    free(utf8);
    return yrc;
}

static yella_ptr_vector* yella_get_dirs(const UChar* const fqpath)
{
    uds cur;
    yella_ptr_vector* vec;

    vec = yella_create_uds_ptr_vector();
    cur = yella_dir_name(fqpath);
    if (u_strcmp(cur, u".") == 0)
    {
        yella_push_back_ptr_vector(vec, cur);
        return vec;
    }
    while (u_strcmp(cur, u"/") != 0 && u_strcmp(cur, u".") != 0)
    {
        yella_push_front_ptr_vector(vec, cur);
        cur = yella_dir_name(cur);
    }
    udsfree(cur);
    return vec;
}

static const UChar* yella_last_not(const UChar* const str, UChar c)
{
    const UChar* result;
    size_t len;

    len = u_strlen(str);
    if (len == 0)
        return NULL;
    result = str + len - 1;
    while (result >= str && *result == c)
        result--;
    return (result < str) ? NULL : result;
}

yella_rc yella_apply_function_to_file_contents(const UChar* const name,
                                               void(*func)(const uint8_t* const, size_t, void*),
                                               void* udata)
{
    uint8_t buf[BUFSIZ];
    int fd;
    char* utf8;
    ssize_t num_read;
    yella_rc yrc;

    utf8 = yella_to_utf8(name);
    fd = open(utf8, O_RDONLY | O_NOFOLLOW);
    if (fd == -1)
    {
        if (errno == EACCES)
            yrc = YELLA_NO_PERMISSION;
        else if (errno == ENOENT)
            yrc = YELLA_DOES_NOT_EXIST;
        else
            yrc = YELLA_FILE_SYSTEM_ERROR;
        CHUCHO_C_ERROR("yella.common",
                       "Could open '%s' for reading: %s",
                       utf8,
                       strerror(errno));
    }
    else
    {
        while (true)
        {
            num_read = read(fd, buf, sizeof(buf));
            if (num_read <= 0)
                break;
            func(buf, num_read, udata);
        }
        if (num_read < 0)
        {
            yrc = YELLA_FILE_SYSTEM_ERROR;
            CHUCHO_C_ERROR("yella.common",
                           "Could read from '%s': %s",
                           utf8,
                           strerror(errno));
        }
        else
        {
            yrc = YELLA_NO_ERROR;
        }
        close(fd);
    }
    free(utf8);
    return yrc;
}

uds yella_base_name(const UChar* const path)
{
    size_t len;
    size_t span;
    const UChar* end;
    const UChar* slash;

    len = u_strlen(path);
    if (len == 0)
        return udsnew(u".");
    span = u_strspn(path, u"/");
    if (span == len)
        return udsnew(u"/");
    end = yella_last_not(path, u'/');
    slash = end;
    while (*slash != u'/' && slash >= path)
        slash--;
    if (slash < path)
        return udsnew(path);
    return udsnewlen(slash + 1, end - slash);
}

yella_rc yella_create_directory(const UChar* const name)
{
    yella_rc result;
    int err;
    int rc;
    char* utf8;

    utf8 = yella_to_utf8(name);
    rc = mkdir(utf8, 0755);
    err = errno;
    if(rc == 0)
    {
        result = YELLA_NO_ERROR;
    }
    else
    {
        CHUCHO_C_ERROR("yella.common",
                       "Could not create directory '%s': %s",
                       utf8,
                       strerror(err));
        if (err == EACCES)
            result = YELLA_NO_PERMISSION;
        else if (err == EEXIST)
            result = YELLA_ALREADY_EXISTS;
        else
            result = YELLA_FILE_SYSTEM_ERROR;
    }
    free(utf8);
    return result;
}

yella_directory_iterator* yella_create_directory_iterator(const UChar* const dir)
{
    yella_directory_iterator* result;
    int err;
    size_t len;
    char* utf8;

    result = malloc(sizeof(yella_directory_iterator));
    utf8 = yella_to_utf8(dir);
    result->dir = opendir(utf8);
    if (result->dir == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.common",
                       "Could not open directory '%s' for reading: %s",
                       utf8,
                       strerror(err));
        free(utf8);
        free(result);
        return NULL;
    }
    free(utf8);
    result->dir_name = udsnew(dir);
    if (result->dir_name[udslen(result->dir_name) - 1] != YELLA_DIR_SEP[0])
        result->dir_name = udscat(result->dir_name, YELLA_DIR_SEP);
    result->fqn = NULL;
    return result;
}

void yella_destroy_directory_iterator(yella_directory_iterator* itor)
{
    closedir(itor->dir);
    udsfree(itor->dir_name);
    udsfree(itor->fqn);
    free(itor);
}

const UChar* yella_directory_iterator_next(yella_directory_iterator* itor)
{
    struct dirent* found;
    UChar* utf16;

    do
    {
        found = readdir(itor->dir);
    } while (found != NULL &&
             (strcmp(found->d_name, ".") == 0 || strcmp(found->d_name, "..") == 0));
    if (found == NULL)
        return NULL;
    udsfree(itor->fqn);
    itor->fqn = udscpy(udsempty(), itor->dir_name);
    utf16 = yella_from_utf8(found->d_name);
    itor->fqn = udscat(itor->fqn, utf16);
    free(utf16);
    return itor->fqn;
}

uds yella_dir_name(const UChar* const path)
{
    size_t len;
    const UChar* end;
    const UChar* slash;
    size_t span;

    len = u_strlen(path);
    if (len == 0)
        return udsnew(u".");
    span = u_strspn(path, u"/");
    if (span == len)
        return udsnew(u"/");
    end = yella_last_not(path, u'/');
    slash = end;
    while (*slash != u'/' && slash >= path)
        slash--;
    if (slash < path)
        return udsnew(u".");
    end = slash;
    while (*end == u'/' && end >= path)
        end--;
    if (end < path)
        return udsnew(u"/");
    return udsnewlen(path, end - path + 1);
}

yella_rc yella_ensure_dir_exists(const UChar* const name)
{
    yella_ptr_vector* dirs;
    size_t i;
    yella_rc rc = YELLA_NO_ERROR;
    const UChar* cur;

    if (name == NULL || name[0] == 0)
        return YELLA_LOGIC_ERROR;
    dirs = yella_get_dirs(name);
    if (yella_ptr_vector_size(dirs) > 0 && u_strcmp(yella_ptr_vector_at(dirs, 0), u".") == 0)
        yella_pop_front_ptr_vector(dirs);
    yella_push_back_ptr_vector(dirs, udsnew(name));
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

bool yella_file_exists(const UChar* const name)
{
    char* utf8;
    int rc;

    utf8 = yella_to_utf8(name);
    rc = access(utf8, F_OK);
    free(utf8);
    return rc == 0;
}

yella_rc yella_file_size(const UChar* const name, size_t* sz)
{
    struct stat info;
    yella_rc result;

    result = stat_impl(name, &info);
    if (result == YELLA_NO_ERROR)
        *sz = info.st_size;
    return result;
}

UChar* yella_getcwd(void)
{
    char* cwd;
    UChar* result;

    cwd = getcwd(NULL, 0);
    result = yella_from_utf8(cwd);
    free(cwd);
    return result;
}

yella_rc yella_get_file_type(const UChar* const name, yella_file_type* tp)
{
    struct stat info;
    yella_rc result;

    result = stat_impl(name, &info);
    if (result == YELLA_NO_ERROR)
    {
        if (S_ISREG(info.st_mode))
            *tp = YELLA_FILE_TYPE_REGULAR;
        else if (S_ISDIR(info.st_mode))
            *tp = YELLA_FILE_TYPE_DIRECTORY;
        else if (S_ISLNK(info.st_mode))
            *tp = YELLA_FILE_TYPE_SYMBOLIC_LINK;
        else if (S_ISSOCK(info.st_mode))
            *tp = YELLA_FILE_TYPE_SOCKET;
        else if (S_ISFIFO(info.st_mode))
            *tp = YELLA_FILE_TYPE_FIFO;
        else if (S_ISCHR(info.st_mode))
            *tp = YELLA_FILE_TYPE_CHARACTER_SPECIAL;
        else if (S_ISBLK(info.st_mode))
            *tp = YELLA_FILE_TYPE_BLOCK_SPECIAL;
#if defined(S_ISWHT)
        else if (S_ISWHT(info.st_mode))
            *tp = YELLA_FILE_TYPE_WHITEOUT;
#endif
    }
    return result;
}

static yella_rc remove_all_impl(const UChar* const name)
{
    yella_directory_iterator* itor = yella_create_directory_iterator(name);
    const UChar* cur = yella_directory_iterator_next(itor);
    struct stat st;
    yella_rc rc = YELLA_NO_ERROR;
    yella_rc cur_rc;
    char* utf8;

    while (cur != NULL)
    {
        utf8 = yella_to_utf8(cur);
        stat(utf8, &st);
        if (S_ISDIR(st.st_mode))
        {
            cur_rc = remove_all_impl(cur);
            if (cur_rc > rc)
                rc = cur_rc;
        }
        else
        {
            if (remove(utf8) != 0)
            {
                CHUCHO_C_ERROR("yella.common",
                               "Unable to remove '%s': %s",
                               utf8,
                               strerror(errno));
                if (YELLA_FILE_SYSTEM_ERROR > rc)
                    rc = YELLA_FILE_SYSTEM_ERROR;
            }
        }
        free(utf8);
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
    utf8 = yella_to_utf8(name);
    if (remove(utf8) != 0)
    {
        CHUCHO_C_ERROR("yella.common",
                       "Unable to remove '%s': %s",
                       utf8,
                       strerror(errno));
        if (YELLA_FILE_SYSTEM_ERROR > rc)
            rc = YELLA_FILE_SYSTEM_ERROR;
    }
    free(utf8);
    return rc;
}

yella_rc yella_remove_all(const UChar* const name)
{
    struct stat st;
    char* utf8;

    utf8 = yella_to_utf8(name);
    if (access(utf8, F_OK) != 0)
    {
        free(utf8);
        return YELLA_NO_ERROR;
    }
    if (stat(utf8, &st) != 0)
    {
        CHUCHO_C_ERROR("yella.common",
                       "Unable to stat '%s': %s",
                       utf8,
                       strerror(errno));
        free(utf8);
        return YELLA_FILE_SYSTEM_ERROR;
    }
    if (S_ISDIR(st.st_mode))
    {
        free(utf8);
        return remove_all_impl(name);
    }
    if (remove(utf8) != 0)
    {
        CHUCHO_C_ERROR("yella.common",
                       "Unable to remove '%s': %s",
                       utf8,
                       strerror(errno));
        free(utf8);
        return YELLA_FILE_SYSTEM_ERROR;
    }
    free(utf8);
    return YELLA_NO_ERROR;
}

