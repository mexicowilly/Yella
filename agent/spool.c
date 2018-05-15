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

#include "agent/spool.h"
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

static const uint64_t YELLA_SPOOL_ID = 0x9311a59001;
static const uint32_t YELLA_VISITED_BIT = 1 << 31;
static const uint32_t YELLA_MAX_MSG_SIZE = UINT32_MAX ^ YELLA_VISITED_BIT;

#define YELLA_EVENT_IS_VISITED(n) (((n) & YELLA_VISITED_BIT) != 0)
#define YELLA_EVENT_MSG_SIZE(n) ((n) & ~YELLA_VISITED_BIT)

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
    bool is_stopped;
    /*
    yella_spool_size_notification size_notification;
    void* size_notification_data;
    yella_mutex* size_guard;
     */
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
        return false;
    }
    yella_lock_mutex(sp->guard);
    num_written = fwrite(&YELLA_SPOOL_ID, 1, sizeof(YELLA_SPOOL_ID), sp->writef);
    yella_unlock_mutex(sp->guard);
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
        return false;
    }
    yella_lock_mutex(sp->guard);
    rc = fflush(sp->writef);
    yella_unlock_mutex(sp->guard);
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
        return false;
    }
    CHUCHO_C_TRACE("yella.spool",
                   "Opened %s for writing",
                   sp->write_file_name);
    return true;
}

static bool init_writer(yella_spool* sp, const yella_saved_state* state)
{
    sp->writef = NULL;
    sp->write_pos.major_seq = yella_saved_state_boot_count(state);
    sp->write_pos.minor_seq = 0;
    sp->write_file_name = NULL;
    return increment_write_spool_partition(sp);
}

static bool increment_read_spool_partition(yella_spool* sp)
{
    int err;
    yella_directory_iterator* itor;
    yella_ptr_vector* to_remove;
    char* candidate;
    uint32_t found_major_seq;
    uint32_t found_minor_seq;
    uint32_t min_major_seq;
    uint32_t min_minor_seq;
    const char* cur;
    char* base;
    int rc;
    size_t i;

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
    min_major_seq = UINT32_MAX;
    min_minor_seq = UINT32_MAX;
    candidate = NULL;
    to_remove = yella_create_ptr_vector();
    itor = yella_create_directory_iterator(yella_settings_get_text("spool-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        base = yella_base_name(cur);
        rc = sscanf(base, "%u-%u", &found_major_seq, &found_minor_seq);
        free(base);
        if (rc == 2 && is_spool_file(cur))
        {
            if ((found_major_seq < min_major_seq ||
                (found_major_seq == min_major_seq && found_minor_seq < min_minor_seq)) &&
                (found_major_seq > sp->read_pos.major_seq ||
                (found_major_seq == sp->read_pos.major_seq && found_minor_seq > sp->read_pos.minor_seq)))
            {
                min_major_seq = found_major_seq;
                min_minor_seq = found_minor_seq;
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
    sp->readf = fopen(candidate, "r+b");
    if (sp->readf == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella.spool",
                       "Unable to open spool file %s for reading: %s",
                       candidate,
                       strerror(err));
        free(candidate);
        return false;
    }
    sp->read_file_name = candidate;
    CHUCHO_C_TRACE("yella.spool",
                   "Opened %s for reading",
                   sp->read_file_name);
    fseek(sp->readf, sizeof(YELLA_SPOOL_ID), SEEK_SET);
    sp->read_pos.major_seq = min_major_seq;
    sp->read_pos.minor_seq = min_minor_seq;
    return true;
}

/**
 * Returns message size in the next unvisited entry and sets
 * the file pointer to the content of the message.
 *
 * On error returns zero. If end-of-file, then feof(readf) will
 * indicate.
 */
static uint32_t advance_to_next_unvisited(yella_spool* sp)
{
    size_t num_read;
    uint32_t msg_size;
    uint16_t i;
    int err;
    size_t visited_count;
    long cur_write_pos;

    visited_count = 0;
    do
    {
        num_read = fread(&msg_size, 1, sizeof(msg_size), sp->readf);
        if (num_read == 0)
        {
            if (feof(sp->readf) != 0)
            {
                yella_lock_mutex(sp->guard);
                if (sp->read_pos.major_seq == sp->write_pos.major_seq &&
                    sp->read_pos.minor_seq == sp->write_pos.minor_seq)
                {
                    cur_write_pos = ftell(sp->writef);
                    CHUCHO_C_TRACE("yella.spool",
                                   "Waiting for more data on file %s",
                                   sp->read_file_name);
                    while (!sp->is_stopped &&
                           sp->read_pos.minor_seq == sp->write_pos.minor_seq &&
                           ftell(sp->writef) == cur_write_pos)
                    {
                        yella_wait_milliseconds_for_condition_variable(sp->was_written_cond, sp->guard, 250);
                    }
                    if (sp->is_stopped)
                    {
                        yella_unlock_mutex(sp->guard);
                        return 0;
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
                    yella_unlock_mutex(sp->guard);
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
                    size_t read_sz = yella_file_size(sp->read_file_name);
                    if (remove(sp->read_file_name) == 0)
                    {
                        sp->size -= read_sz;
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
                    yella_lock_mutex(sp->guard);
                }
                else
                {
                    yella_unlock_mutex(sp->guard);
                    CHUCHO_C_ERROR("yella.spool",
                                   "The spool reader at (%lu, %lu) has read beyond the spool writer at (%lu, %lu)",
                                   (unsigned long)sp->read_pos.major_seq,
                                   (unsigned long)sp->read_pos.minor_seq,
                                   (unsigned long)sp->write_pos.major_seq,
                                   (unsigned long)sp->write_pos.minor_seq);
                    return 0;
                }
                yella_unlock_mutex(sp->guard);
            }
            else
            {
                CHUCHO_C_ERROR("yella.spool",
                               "Zero bytes were read from %s without reaching the end of file.",
                               sp->read_file_name);
                return 0;
            }
        }
        else if (num_read < sizeof(msg_size))
        {
            CHUCHO_C_ERROR("yella.spool",
                           "An anomalous short read occurred on %s. Going to try again.",
                           sp->read_file_name);
            fseek(sp->readf, -(long)num_read, SEEK_CUR);
            num_read = 0;
        }
        else
        {
            if (YELLA_EVENT_IS_VISITED(msg_size))
            {
                /* fast forward to next */
                CHUCHO_C_TRACE("yella.spool",
                               "Fast forwarding over already visited event (%zu) in %s",
                               ++visited_count,
                               sp->read_file_name);
                for (i = 0; i < YELLA_EVENT_MSG_SIZE(msg_size); i++)
                {
                    num_read = fread(&msg_size, 1, sizeof(msg_size), sp->readf);
                    if (num_read < sizeof(msg_size))
                    {
                        /* error */
                    }
                    fseek(sp->readf, msg_size, SEEK_CUR);
                }
            }
            else
            {
                /* we won */
                msg_size = YELLA_EVENT_MSG_SIZE(msg_size);
            }
        }
    } while (num_read == 0 || YELLA_EVENT_IS_VISITED(msg_size));
    return msg_size;
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
    sp->is_stopped = false;
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
    sp->is_stopped = true;
    yella_unlock_mutex(sp->guard);
    fclose(sp->readf);
    fclose(sp->writef);
    yella_destroy_mutex(sp->guard);
    yella_destroy_condition_variable(sp->was_written_cond);
    free(sp->write_file_name);
    free(sp->read_file_name);
    free(sp);
}

uint8_t* yella_pop_spool(yella_spool* sp, size_t* sz)
{
    uint32_t loc_sz;
    uint8_t* msg = NULL;

    loc_sz = advance_to_next_unvisited(sp);
    if (loc_sz == 0)
    {
        /* error */
    }
    else
    {
        msg = malloc(loc_sz);
        if (fread(msg, 1, loc_sz, sp->readf) == loc_sz)
        {
            *sz = loc_sz;
            sp->size -= loc_sz;
        }
        else
        {
            /* error */
            free(msg);
            msg = NULL;
        }
    }
    return msg;
}

bool yella_push_spool(yella_spool* sp, const uint8_t* msg, size_t sz)
{
    uint32_t len;
    size_t i;
    size_t num_written;

    if (ftell(sp->writef) >= *yella_settings_get_uint("max-spool-partition"))
    {
        if (!increment_write_spool_partition(sp))
            return false;
    }
    len = (uint32_t)sz;
    assert(len <= YELLA_MAX_MSG_SIZE);
    yella_lock_mutex(sp->guard);
    num_written = fwrite(&len, 1, sizeof(len), sp->writef);
    if (num_written != sizeof(len))
    {
        CHUCHO_C_ERROR("yella.spool",
                       "Error writing message count to spool file. The spooler cannot continue.");
        yella_unlock_mutex(sp->guard);
        return false;
    }
    num_written = fwrite(msg, 1, len, sp->writef);
    if (num_written != len)
    {
        CHUCHO_C_ERROR("yella.spool",
                       "Error writing message data (%zu) to spool file. The spooler cannot continue.",
                       i);
        yella_unlock_mutex(sp->guard);
        return false;
    }
    fflush(sp->writef);
    sp->size += sizeof(uint32_t) + sz;
    yella_signal_condition_variable(sp->was_written_cond);
    yella_unlock_mutex(sp->guard);
    return true;
}
