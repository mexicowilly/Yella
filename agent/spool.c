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

#include "spool.h"
#include "common/file.h"
#include "common/settings.h"
#include "common/ptr_vector.h"
#include "common/thread.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

const uint64_t YELLA_SPOOL_ID = 0x9311a59001;
const size_t YELLA_MAX_MSG_COUNT = 0x0fff;
const uint16_t YELLA_VISITED_BIT = 1 << 15;

#define YELLA_EVENT_IS_VISITED(n) (((n) & YELLA_VISITED_BIT) != 0)
#define YELLA_EVENT_MSG_COUNT(n) ((n) & ~YELLA_VISITED_BIT)

typedef struct spool_pos
{
    uint32_t major_seq;
    uint32_t minor_seq;
} spool_pos;

struct yella_spool
{
    spool_pos write_pos;
    FILE* writef;
    spool_pos read_pos;
    FILE* readf;
    yella_mutex* guard;
    yella_condition_variable* was_written_cond;
    char* read_file_name;
    char* write_file_name;
    uintmax_t size;
};

static bool is_spool_file(const char* const name)
{
    FILE* f;
    uint64_t id;
    bool result;

    result = false;
    f = fopen(name, "rb");
    if (f != NULL)
    {
        if (fread(&id, 1, sizeof(id), f) == sizeof(id) &&
            id == YELLA_SPOOL_ID)
        {
            result = true;
        }
    }
    fclose(f);
    return result;
}

static char* spool_file_name(uint32_t major_seq, uint32_t minor_seq)
{
    return yella_sprintf("%s%s%lu-%lu.yella.spool",
                         yella_settings_get_text("spool-dir"),
                         YELLA_DIR_SEP,
                         (unsigned long)major_seq,
                         (unsigned long)minor_seq);
}

static bool increment_write_spool_partition(yella_spool* sp)
{
    char* fname;
    int err;
    size_t num_written;
    int rc;

    yella_lock_mutex(sp->guard);
    if (sp->writef != NULL && fclose(sp->writef) != 0)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.spool",
                       "Error closing write stream: %s",
                       strerror(err));
    }
    free(sp->write_file_name);
    sp->write_file_name = spool_file_name(sp->write_pos.major_seq, ++sp->write_pos.minor_seq);
    sp->writef = fopen(sp->write_file_name, "wb");
    if (sp->writef == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.spool",
                       "Unable to open spool file %s for writing: %s",
                       sp->write_file_name,
                       strerror(err));
        free(sp->write_file_name);
        sp->write_file_name = NULL;
        yella_unlock_mutex(sp->guard);
        return false;
    }
    num_written = fwrite(&YELLA_SPOOL_ID, 1, sizeof(YELLA_SPOOL_ID), sp->writef);
    if (num_written != sizeof(YELLA_SPOOL_ID))
    {
        err = errno;
        CHUCHO_C_ERROR("yella.spool",
                       "Unable to write to spool file %s",
                       fname);
        fclose(sp->writef);
        sp->writef = NULL;
        free(sp->write_file_name);
        sp->write_file_name = NULL;
        yella_unlock_mutex(sp->guard);
        return false;
    }
    rc = fflush(sp->writef);
    if (rc != 0)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.spool",
                       "Unable to flush spool file %s",
                       fname);
        fclose(sp->writef);
        sp->writef = NULL;
        free(sp->write_file_name);
        sp->write_file_name = NULL;
        yella_unlock_mutex(sp->guard);
        return false;
    }
    CHUCHO_C_TRACE("yella.spool",
                   "Opened %s for writing",
                   sp->write_file_name);
    yella_unlock_mutex(sp->guard);
    return true;
}

static bool init_writer(yella_spool* sp, const yella_saved_state* state)
{
    sp->writef = NULL;
    sp->write_pos.major_seq = state->boot_count;
    sp->write_pos.minor_seq = 0;
    sp->write_file_name = NULL;
    return increment_write_spool_partition(sp);
}

char* find_oldest_file(yella_spool* sp, spool_pos* pos)
{
    yella_directory_iterator* itor;
    yella_ptr_vector* to_remove;
    char* candidate;
    spool_pos found_pos;
    const char* cur;
    char* base;
    int rc;
    int i;

    pos->major_seq = UINT32_MAX;
    pos->minor_seq = UINT32_MAX;
    candidate = NULL;
    to_remove = yella_create_ptr_vector();
    itor = yella_create_directory_iterator(yella_settings_get_text("spool-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        base = yella_base_name(cur);
        rc = sscanf(base, "%u-%u", &found_pos.major_seq, &found_pos.minor_seq);
        free(base);
        if (rc == 2 && is_spool_file(cur))
        {
            if ((found_pos.major_seq < pos->major_seq ||
                 (found_pos.major_seq == pos->major_seq && found_pos.minor_seq < pos->minor_seq)) &&
                (found_pos.major_seq > sp->read_pos.major_seq ||
                 (found_pos.major_seq == sp->read_pos.major_seq && found_pos.minor_seq > sp->read_pos.minor_seq)))
            {
                *pos = found_pos;
                if (candidate != NULL)
                    free(candidate);
                candidate = yella_text_dup(cur);
            }
        }
        else
        {
            CHUCHO_C_WARN("yella.spool",
                          "Found unexpected spool file %s. It is being removed.",
                          cur);
            yella_push_back_ptr_vector(to_remove, yella_text_dup(cur));
        }
        cur = yella_directory_iterator_next(itor);
    }
    for (i = 0; i < yella_ptr_vector_size(to_remove); i++)
        remove(yella_ptr_vector_at(to_remove, i));
    yella_destroy_ptr_vector(to_remove);
    return candidate;
}

static bool increment_read_spool_partition(yella_spool* sp)
{
    int err;
    int rc;
    char* found;
    spool_pos found_pos;

    yella_lock_mutex(sp->guard);
    if (sp->read_file_name != NULL)
    {
        CHUCHO_C_TRACE("yella.spool",
                       "Closing read stream %s",
                       sp->read_file_name);
        free(sp->read_file_name);
        sp->read_file_name = NULL;
    }
    if (sp->readf != NULL && fclose(sp->readf) != 0)
    {
        err = errno;
        CHUCHO_C_WARN("yella.spool",
                      "Error closing read stream: %s",
                      strerror(err));
    }
    sp->readf = NULL;
    found = find_oldest_file(sp, &found_pos);
    sp->readf = fopen(found, "r+b");
    if (sp->readf == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.spool",
                       "Unable to open spool file %s for reading: %s",
                       found,
                       strerror(err));
        free(found);
        return false;
    }
    sp->read_file_name = found;
    CHUCHO_C_TRACE("yella.spool",
                   "Opened %s for reading",
                   sp->read_file_name);
    fseek(sp->readf, sizeof(YELLA_SPOOL_ID), SEEK_SET);
    sp->read_pos = found_pos;
    yella_unlock_mutex(sp->guard);
    return true;
}

/**
 * Returns message count in the next unvisited entry and sets
 * the file pointer to the uint32_t that has the size of the
 * first message part.
 *
 * On error returns zero. If end-of-file, then feof(readf) will
 * indicate.
 *
 * Guard is locked on entry
 */
static uint16_t advance_to_next_unvisited(yella_spool* sp)
{
    uint16_t msg_count;
    size_t num_read;
    uint32_t part_size;
    uint16_t i;
    int err;
    size_t visited_count;
    long cur_write_pos;

    visited_count = 0;
    do
    {
        num_read = fread(&msg_count, 1, sizeof(msg_count), sp->readf);
        if (num_read == 0)
        {
            if (feof(sp->readf) != 0)
            {
                if (sp->read_pos.major_seq == sp->write_pos.major_seq &&
                    sp->read_pos.minor_seq == sp->write_pos.minor_seq)
                {
                    cur_write_pos = ftell(sp->writef);
                    CHUCHO_C_TRACE("yella.spool",
                                   "Waiting for more data on file %s",
                                   sp->read_file_name);
                    while (sp->read_pos.minor_seq == sp->write_pos.minor_seq &&
                           ftell(sp->writef) == cur_write_pos)
                    {
                        yella_wait_milliseconds_for_condition_variable(sp->was_written_cond, sp->guard, 250);
                    }
                    clearerr(sp->readf);
                    CHUCHO_C_TRACE("yella.spool",
                                   "Ready to read more data from file %s",
                                   sp->read_file_name);
                }
                else if (sp->read_pos.major_seq < sp->write_pos.major_seq ||
                         (sp->read_pos.major_seq == sp->write_pos.major_seq &&
                          sp->read_pos.minor_seq < sp->write_pos.minor_seq))
                {
                    /* advance to next */
                    CHUCHO_C_TRACE("yella.spool",
                                   "Done reading from %s. It is being removed.",
                                   sp->read_file_name);
                    if (fclose(sp->readf) != 0)
                    {
                        err = errno;
                        CHUCHO_C_WARN("yella.spool",
                                      "Error closing read stream %s: %s",
                                      sp->read_file_name,
                                      strerror(err));
                    }
                    sp->readf = NULL;
                    if (remove(sp->read_file_name) == 0)
                    {
                        sp->size -= yella_file_size(sp->read_file_name);
                    }
                    else
                    {
                        err = errno;
                        CHUCHO_C_WARN("yella.spool",
                                      "Error removing read stream %s: %s",
                                      sp->read_file_name,
                                      strerror(err));
                    }
                    if (!increment_read_spool_partition(sp))
                        return 0;
                }
                else
                {
                    CHUCHO_C_ERROR("yella.spool",
                                   "The spool reader at (%lu, %lu) has read beyond the spool writer at (%lu, %lu)",
                                   (unsigned long)sp->read_pos.major_seq,
                                   (unsigned long)sp->read_pos.minor_seq,
                                   (unsigned long)sp->write_pos.major_seq,
                                   (unsigned long)sp->write_pos.minor_seq);
                    return 0;
                }
            }
            else
            {
                CHUCHO_C_ERROR("yella.spool",
                               "Zero bytes were read from %s without reaching the end of file.",
                               sp->read_file_name);
                return 0;
            }
        }
        else if (num_read < sizeof(msg_count))
        {
            CHUCHO_C_ERROR("yella.spool",
                           "An anomalous short read occurred on %s. Going to try again.",
                           sp->read_file_name);
            fseek(sp->readf, -(long)num_read, SEEK_CUR);
            num_read = 0;
        }
        else
        {
            if (YELLA_EVENT_IS_VISITED(msg_count))
            {
                /* fast forward to next */
                CHUCHO_C_TRACE("yella.spool",
                               "Fast forwarding over already visited event (%zu) in %s",
                               ++visited_count,
                               sp->read_file_name);
                for (i = 0; i < YELLA_EVENT_MSG_COUNT(msg_count); i++)
                {
                    num_read = fread(&part_size, 1, sizeof(part_size), sp->readf);
                    if (num_read < sizeof(part_size))
                    {
                        /* error */
                    }
                    fseek(sp->readf, part_size, SEEK_CUR);
                }
            }
            else
            {
                /* we won */
                msg_count = YELLA_EVENT_MSG_COUNT(msg_count);
            }
        }
    } while (num_read == 0 || YELLA_EVENT_IS_VISITED(msg_count));
    return msg_count;
}

static bool init_reader(yella_spool* sp)
{
    sp->readf = NULL;
    sp->read_pos.major_seq = 0;
    sp->read_pos.minor_seq = 0;
    sp->read_file_name = NULL;
    return increment_read_spool_partition(sp);
}

static uintmax_t current_spool_size()
{
    yella_directory_iterator* itor;
    uintmax_t result;
    const char* cur;

    result = 0;
    itor = yella_create_directory_iterator(yella_settings_get_text("spool-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        if (is_spool_file(cur))
            result += yella_file_size(cur);
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
    return result;
}

yella_spool* yella_create_spool(const yella_saved_state* state)
{
    yella_spool* sp;

    sp = malloc(sizeof(yella_spool));
    sp->guard = yella_create_mutex();
    sp->was_written_cond = yella_create_condition_variable();
    sp->size = current_spool_size();
    if (!init_writer(sp, state) || !init_reader(sp))
    {
        yella_destroy_condition_variable(sp->was_written_cond);
        yella_destroy_mutex(sp->guard);
        free(sp->read_file_name);
        free(sp->write_file_name);
        free(sp);
        return NULL;
    }
    return sp;
}

void yella_destroy_spool(yella_spool* sp)
{
    yella_lock_mutex(sp->guard);
    fclose(sp->readf);
    fclose(sp->writef);
    yella_destroy_mutex(sp->guard);
    yella_destroy_condition_variable(sp->was_written_cond);
    free(sp->write_file_name);
    free(sp->read_file_name);
    yella_unlock_mutex(sp->guard);
    free(sp);
}

yella_msg_part* yella_spool_pop(yella_spool* sp, size_t milliseconds_to_wait, size_t* count)
{
    yella_msg_part* result = NULL;
    uint16_t msg_count;
    uint16_t i;
    uint32_t msg_size;

    *count = 0;
    yella_lock_mutex(sp->guard);
    if (sp->size > 0 ||
        yella_wait_milliseconds_for_condition_variable(sp->was_written_cond, sp->guard, milliseconds_to_wait))
    {
        msg_count = advance_to_next_unvisited(sp);
        *count = msg_count;
        if (msg_count > 0)
        {
            result = calloc(msg_count, sizeof(yella_msg_part));
            for (i = 0; i < msg_count; i++)
            {
                if (fread(&msg_size, sizeof(msg_size), 1, sp->readf) != sizeof(msg_size))
                {
                    /* error */
                }
                result[i].size = msg_size;
                result[i].data = malloc(msg_size);
                if (fread(result[i].data, 1, msg_size, sp->readf) != msg_size)
                {
                    /* error */
                }
            }
        }
    }
    yella_unlock_mutex(sp->guard);
    return result;
}

bool yella_spool_push(yella_spool* sp, yella_msg_part* msgs, size_t count)
{
    uint16_t num;
    uint32_t len;
    size_t i;
    size_t num_written;

    yella_lock_mutex(sp->guard);
    if (ftell(sp->writef) >= *yella_settings_get_uint("max-spool-partition"))
    {
        if (!increment_write_spool_partition(sp))
        {
            yella_unlock_mutex(sp->guard);
            return false;
        }
    }
    num = (uint16_t)count;
    assert(num <= YELLA_MAX_MSG_COUNT);
    num_written = fwrite(&num, 1, sizeof(num), sp->writef);
    if (num_written != sizeof(num))
    {
        CHUCHO_C_ERROR("yella.spool",
                       "Error writing message count to spool file. The spooler cannot continue.");
        yella_unlock_mutex(sp->guard);
        return false;
    }
    for (i = 0; i < count; i++)
    {
        len = msgs[i].size;
        num_written = fwrite(&len, 1, sizeof(len), sp->writef);
        if (num_written != sizeof(len))
        {
            CHUCHO_C_ERROR("yella.spool",
                           "Error writing message length (%zu) to spool file. The spooler cannot continue.",
                           i);
            yella_unlock_mutex(sp->guard);
            return false;
        }
        num_written = fwrite(msgs[i].data, 1, len, sp->writef);
        if (num_written != len)
        {
            CHUCHO_C_ERROR("yella.spool",
                           "Error writing message data (%zu) to spool file. The spooler cannot continue.",
                           i);
            yella_unlock_mutex(sp->guard);
            return false;
        }
    }
    fflush(sp->writef);
    yella_signal_condition_variable(sp->was_written_cond);
    yella_unlock_mutex(sp->guard);
    return true;
}
