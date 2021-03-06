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
#include "common/uds_util.h"
#include <unicode/ustdio.h>
#include <chucho/log.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <inttypes.h>

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

struct spool
{
    spool_pos write_pos;
    FILE* writef;
    spool_pos read_pos;
    FILE* readf;
    yella_mutex* guard;
    yella_condition_variable* was_written_cond;
    uds read_file_name;
    uds write_file_name;
    spool_stats stats;
    size_t total_event_bytes_written;
    chucho_logger_t* lgr;
};

static bool is_spool_file(const UChar* const name)
{
    FILE* f;
    uint64_t id;
    bool result;
    char* utf8;

    result = false;
    utf8 = yella_to_utf8(name);
    f = fopen(utf8, "rb");
    free(utf8);
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

static uds spool_file_name(uint32_t major_seq, uint32_t minor_seq)
{
    return udscatprintf(udsempty(),
                        u"%S%S%lu-%lu.yella.spool",
                        yella_settings_get_dir(u"agent", u"spool-dir"),
                        YELLA_DIR_SEP,
                        (unsigned long)major_seq,
                        (unsigned long)minor_seq);
}

static bool increment_write_spool_partition(spool* sp)
{
    int err;
    size_t num_written;
    int rc;
    char* utf8;

    if (sp->writef != NULL && fclose(sp->writef) != 0)
    {
        err = errno;
        CHUCHO_C_ERROR_L(sp->lgr,
                         "Error closing write stream: %s",
                         strerror(err));
    }
    udsfree(sp->write_file_name);
    sp->write_file_name = spool_file_name(sp->write_pos.major_seq, ++sp->write_pos.minor_seq);
    utf8 = yella_to_utf8(sp->write_file_name);
    sp->writef = fopen(utf8, "wb");
    if (sp->writef == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR_L(sp->lgr,
                         "Unable to open spool file %s for writing: %s",
                         utf8,
                         strerror(err));
        free(utf8);
        udsfree(sp->write_file_name);
        sp->write_file_name = NULL;
        return false;
    }
    num_written = fwrite(&YELLA_SPOOL_ID, 1, sizeof(YELLA_SPOOL_ID), sp->writef);
    if (num_written != sizeof(YELLA_SPOOL_ID))
    {
        err = errno;
        CHUCHO_C_ERROR_L(sp->lgr,
                         "Unable to write to spool file %s",
                         utf8);
        free(utf8);
        fclose(sp->writef);
        sp->writef = NULL;
        udsfree(sp->write_file_name);
        sp->write_file_name = NULL;
        return false;
    }
    rc = fflush(sp->writef);
    if (rc != 0)
    {
        err = errno;
        CHUCHO_C_ERROR_L(sp->lgr,
                         "Unable to flush spool file %s",
                         utf8);
        free(utf8);
        fclose(sp->writef);
        sp->writef = NULL;
        udsfree(sp->write_file_name);
        sp->write_file_name = NULL;
        return false;
    }
    sp->stats.current_size += num_written;
    ++sp->stats.files_created;
    CHUCHO_C_TRACE_L(sp->lgr,
                     "Opened %s for writing",
                     utf8);
    free(utf8);
    return true;
}

static uds find_file(spool* sp, spool_pos* pos, bool (*cmp_func)(spool* sp, const spool_pos* found_pos, const spool_pos* prev_pos))
{
    yella_directory_iterator* itor;
    yella_ptr_vector* to_remove;
    uds candidate;
    spool_pos found_pos;
    const UChar* cur;
    uds base;
    int rc;
    int i;
    char* utf8;

    candidate = NULL;
    to_remove = yella_create_uds_ptr_vector();
    itor = yella_create_directory_iterator(yella_settings_get_dir(u"agent", u"spool-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        base = yella_base_name(cur);
        rc = u_sscanf_u(base, u"%u-%u", &found_pos.major_seq, &found_pos.minor_seq);
        udsfree(base);
        if (rc == 2 && is_spool_file(cur))
        {
            if (cmp_func(sp, &found_pos, pos))
            {
                *pos = found_pos;
                udsfree(candidate);
                candidate = udsnew(cur);
            }
        }
        else
        {
            utf8 = yella_to_utf8(cur);
            CHUCHO_C_WARN_L(sp->lgr,
                            "Found unexpected spool file %s. It is being removed.",
                            utf8);
            free(utf8);
            yella_push_back_ptr_vector(to_remove, udsnew(cur));
        }
        cur = yella_directory_iterator_next(itor);
    }
    for (i = 0; i < yella_ptr_vector_size(to_remove); i++)
        remove(yella_ptr_vector_at(to_remove, i));
    yella_destroy_ptr_vector(to_remove);
    yella_destroy_directory_iterator(itor);
    return candidate;
}

static bool cmp_newest(spool* sp, const spool_pos* found_pos, const spool_pos* prev_pos)
{
    return (found_pos->major_seq > prev_pos->major_seq ||
            (found_pos->major_seq == prev_pos->major_seq && found_pos->minor_seq > prev_pos->minor_seq));

}

uds find_newest_file(spool* sp, spool_pos* pos)
{
    pos->major_seq = 0;
    pos->minor_seq = 0;
    return find_file(sp, pos, cmp_newest);
}

static bool init_writer(spool* sp)
{
    uds newest;
    spool_pos pos;

    sp->writef = NULL;
    newest = find_newest_file(sp, &pos);
    if (newest == NULL)
    {
        sp->write_pos.major_seq = 1;
    }
    else
    {
        sp->write_pos.major_seq = pos.major_seq + 1;
        udsfree(newest);
    }
    sp->write_pos.minor_seq = 0;
    sp->write_file_name = NULL;
    return increment_write_spool_partition(sp);
}

static bool cmp_oldest(spool* sp, const spool_pos* found_pos, const spool_pos* prev_pos)
{
    return ((found_pos->major_seq < prev_pos->major_seq ||
            (found_pos->major_seq == prev_pos->major_seq && found_pos->minor_seq < prev_pos->minor_seq)) &&
           (found_pos->major_seq > sp->read_pos.major_seq ||
            (found_pos->major_seq == sp->read_pos.major_seq && found_pos->minor_seq > sp->read_pos.minor_seq)));
}

uds find_oldest_file(spool* sp, spool_pos* pos)
{
    pos->major_seq = UINT32_MAX;
    pos->minor_seq = UINT32_MAX;
    return find_file(sp, pos, cmp_oldest);
}

static void cull(spool* sp)
{
    spool_pos pos;
    uds oldest = find_oldest_file(sp, &pos);
    size_t sz = 0;
    char* utf8;

    yella_file_size(oldest, &sz);
    utf8 = yella_to_utf8(oldest);
    if (remove(utf8) == 0)
    {
        sp->stats.current_size -= sz;
        sp->stats.bytes_culled += sz;
        ++sp->stats.files_destroyed;
        ++sp->stats.cull_events;
        CHUCHO_C_WARN_L(sp->lgr,
                        "The spool filled, so the oldest bytes were culled. File %s: %zu bytes",
                        utf8,
                        sz);
        free(utf8);
    }
    else
    {
        free(utf8);
        utf8 = yella_to_utf8(sp->read_file_name);
        CHUCHO_C_ERROR_L(sp->lgr,
                         "Could not remove %s: %s",
                         utf8,
                         strerror(errno));
        free(utf8);
    }
    udsfree(oldest);
}

static bool increment_read_spool_partition(spool* sp)
{
    int err;
    int rc;
    uds found;
    spool_pos found_pos;
    char* utf8;

    if (sp->read_file_name != NULL)
    {
        if (chucho_logger_permits(sp->lgr, CHUCHO_TRACE))
        {
            utf8 = yella_to_utf8(sp->read_file_name);
            CHUCHO_C_TRACE_L(sp->lgr,
                             "Closing read stream %s",
                             utf8);
            free(utf8);
        }
        udsfree(sp->read_file_name);
        sp->read_file_name = NULL;
    }
    if (sp->readf != NULL && fclose(sp->readf) != 0)
    {
        err = errno;
        CHUCHO_C_WARN_L(sp->lgr,
                        "Error closing read stream: %s",
                        strerror(err));
    }
    sp->readf = NULL;
    found = find_oldest_file(sp, &found_pos);
    utf8 = yella_to_utf8(found);
    sp->readf = fopen(utf8, "r+b");
    if (sp->readf == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR_L(sp->lgr,
                         "Unable to open spool file %s for reading: %s",
                         utf8,
                         strerror(err));
        free(utf8);
        udsfree(found);
        return false;
    }
    sp->read_file_name = found;
    CHUCHO_C_TRACE_L(sp->lgr,
                     "Opened %s for reading",
                     utf8);
    free(utf8);
    fseek(sp->readf, sizeof(YELLA_SPOOL_ID), SEEK_SET);
    sp->read_pos = found_pos;
    return true;
}

static int compare_write_to_read(spool* sp)
{
    int32_t diff;

    if (sp->write_pos.major_seq > sp->read_pos.major_seq)
    {
        return 1;
    }
    else if (sp->write_pos.major_seq == sp->read_pos.major_seq)
    {
        diff = sp->write_pos.minor_seq - sp->read_pos.minor_seq;
        return (diff == 0) ? ftell(sp->writef) - ftell(sp->readf) : diff;
    }
    else
    {
        return -1;
    }
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
static uint16_t advance_to_next_unvisited(spool* sp)
{
    uint16_t msg_count;
    size_t num_read;
    uint32_t part_size;
    uint16_t i;
    int err;
    size_t visited_count = 0;
    long cur_write_pos;
    size_t sz;
    int diff;
    char* utf8;

    do
    {
        num_read = fread(&msg_count, 1, sizeof(msg_count), sp->readf);
        if (num_read == 0)
        {
            if (feof(sp->readf) != 0)
            {
                diff = compare_write_to_read(sp);
                if (diff > 0)
                {
                    utf8 = yella_to_utf8(sp->read_file_name);
                    /* advance to next */
                    CHUCHO_C_TRACE_L(sp->lgr,
                                     "Done reading from %s. It is being removed.",
                                     utf8);
                    if (fclose(sp->readf) != 0)
                    {
                        err = errno;
                        CHUCHO_C_WARN_L(sp->lgr,
                                        "Error closing read stream %s: %s",
                                        utf8,
                                        strerror(err));
                    }
                    sp->readf = NULL;
                    sz = 0;
                    yella_file_size(sp->read_file_name, &sz);
                    if (remove(utf8) == 0)
                    {
                        sp->stats.current_size -= sz;
                        ++sp->stats.files_destroyed;
                        CHUCHO_C_TRACE_L(sp->lgr,
                                         "Removed read stream %s",
                                         utf8);
                    }
                    else
                    {
                        err = errno;
                        CHUCHO_C_WARN_L(sp->lgr,
                                        "Error removing read stream %s: %s",
                                        utf8,
                                        strerror(err));
                    }
                    free(utf8);
                    udsfree(sp->read_file_name);
                    sp->read_file_name = NULL;
                    if (!increment_read_spool_partition(sp))
                        return 0;
                }
                else if (diff < 0)
                {
                    CHUCHO_C_ERROR_L(sp->lgr,
                                     "The spool reader at (%lu, %lu) has read beyond the spool writer at (%lu, %lu)",
                                     (unsigned long)sp->read_pos.major_seq,
                                     (unsigned long)sp->read_pos.minor_seq,
                                     (unsigned long)sp->write_pos.major_seq,
                                     (unsigned long)sp->write_pos.minor_seq);
                    return 0;
                }
                else
                {
                    return 0;
                }
            }
            else
            {
                utf8 = yella_to_utf8(sp->read_file_name);
                CHUCHO_C_ERROR_L(sp->lgr,
                                 "Zero bytes were read from %s without reaching the end of file.",
                                 utf8);
                free(utf8);
                return 0;
            }
        }
        else if (num_read < sizeof(msg_count))
        {
            utf8 = yella_to_utf8(sp->read_file_name);
            CHUCHO_C_ERROR_L(sp->lgr,
                             "An anomalous short read occurred on %s. Going to try again.",
                             utf8);
            free(utf8);
            fseek(sp->readf, -(long)num_read, SEEK_CUR);
            num_read = 0;
        }
        else
        {
            if (YELLA_EVENT_IS_VISITED(msg_count))
            {
                /* fast forward to next */
                if (chucho_logger_permits(sp->lgr, CHUCHO_TRACE))
                {
                    utf8 = yella_to_utf8(sp->read_file_name);
                    CHUCHO_C_TRACE_L(sp->lgr,
                                     "Fast forwarding over already visited event (%zu) in %s",
                                     ++visited_count,
                                     utf8);
                    free(utf8);
                }
                for (i = 0; i < YELLA_EVENT_MSG_COUNT(msg_count); i++)
                {
                    num_read = fread(&part_size, 1, sizeof(part_size), sp->readf);
                    if (num_read < sizeof(part_size))
                    {
                        CHUCHO_C_ERROR_L(sp->lgr,
                                         "Unable to fast forward.");
                        return 0;
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

static bool init_reader(spool* sp)
{
    sp->readf = NULL;
    sp->read_pos.major_seq = 0;
    sp->read_pos.minor_seq = 0;
    sp->read_file_name = NULL;
    return increment_read_spool_partition(sp);
}

static size_t current_spool_size()
{
    yella_directory_iterator* itor;
    size_t result;
    const UChar* cur;
    size_t cur_size;
    yella_ptr_vector* to_remove;
    size_t i;

    result = 0;
    to_remove = yella_create_uds_ptr_vector();
    itor = yella_create_directory_iterator(yella_settings_get_dir(u"agent", u"spool-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        if (is_spool_file(cur))
        {
            cur_size = 0;
            yella_file_size(cur, &cur_size);
            if (cur_size == sizeof(YELLA_SPOOL_ID))
                yella_push_back_ptr_vector(to_remove, udsnew(cur));
        }
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
    for (i = 0; i < yella_ptr_vector_size(to_remove); i++)
        yella_remove_file(yella_ptr_vector_at(to_remove, i));
    if (yella_ptr_vector_size(to_remove) > 0)
        CHUCHO_C_INFO("spool", "Removed %zu empty spool files", yella_ptr_vector_size(to_remove));
    yella_destroy_ptr_vector(to_remove);
    itor = yella_create_directory_iterator(yella_settings_get_dir(u"agent", u"spool-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        if (is_spool_file(cur))
        {
            cur_size = 0;
            yella_file_size(cur, &cur_size);
            result += cur_size;
        }
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
    return result;
}

spool* create_spool(void)
{
    spool* sp;
    yella_rc yrc;
    char* utf8;

    yrc = yella_ensure_dir_exists(yella_settings_get_dir(u"agent", u"spool-dir"));
    if (yrc != YELLA_NO_ERROR)
    {
        utf8 = yella_to_utf8(yella_settings_get_dir(u"agent", u"spool-dir"));
        CHUCHO_C_FATAL("spool",
                       "Unable to create the directory %s: %s",
                       utf8,
                       yella_strerror(yrc));
        free(utf8);
        return NULL;
    }
    sp = calloc(1, sizeof(spool));
    sp->lgr = chucho_get_logger("spool");
    sp->guard = yella_create_mutex();
    sp->was_written_cond = yella_create_condition_variable();
    sp->stats.current_size = current_spool_size();
    sp->stats.max_partition_size = *yella_settings_get_byte_size(u"agent", u"max-spool-partition-size");
    sp->stats.max_partitions = *yella_settings_get_uint(u"agent", u"max-spool-partitions");
    sp->stats.smallest_event_size = (size_t)-1;
    if (!init_writer(sp) || !init_reader(sp))
    {
        yella_destroy_condition_variable(sp->was_written_cond);
        yella_destroy_mutex(sp->guard);
        udsfree(sp->read_file_name);
        udsfree(sp->write_file_name);
        chucho_release_logger(sp->lgr);
        free(sp);
        return NULL;
    }
    return sp;
}

void destroy_spool(spool* sp)
{
    long woff;
    char* utf8;

    if (sp != NULL)
    {
        yella_lock_mutex(sp->guard);
        fclose(sp->readf);
        udsfree(sp->read_file_name);
        woff = ftell(sp->writef);
        fclose(sp->writef);
        if (woff == sizeof(YELLA_SPOOL_ID))
        {
            utf8 = yella_to_utf8(sp->write_file_name);
            remove(utf8);
            free(utf8);
        }
        udsfree(sp->write_file_name);
        yella_unlock_mutex(sp->guard);
        yella_destroy_mutex(sp->guard);
        yella_destroy_condition_variable(sp->was_written_cond);
        chucho_release_logger(sp->lgr);
        free(sp);
    }
}

bool spool_empty_of_messages(spool* sp)
{
    bool result;

    yella_lock_mutex(sp->guard);
    result = sp->stats.current_size == sizeof(YELLA_SPOOL_ID);
    yella_unlock_mutex(sp->guard);
    return result;
}

spool_stats spool_get_stats(spool* sp)
{
    spool_stats stats;

    yella_lock_mutex(sp->guard);
    sp->stats.average_event_size = sp->stats.events_written == 0 ?
        0 : (sp->total_event_bytes_written / sp->stats.events_written);
    stats = sp->stats;
    yella_unlock_mutex(sp->guard);
    return stats;
}

yella_rc spool_pop(spool* sp,
                   size_t milliseconds_to_wait,
                   yella_message_part** parts,
                   size_t* count)
{
    uint16_t msg_count;
    uint16_t i;
    uint32_t msg_size;
    yella_rc yrc = YELLA_NO_ERROR;
    long msg_off;
    long cur_off;
    char* utf8;

    *parts = NULL;
    *count = 0;
    yella_lock_mutex(sp->guard);
    if (compare_write_to_read(sp) > 0 ||
        yella_wait_milliseconds_for_condition_variable(sp->was_written_cond, sp->guard, milliseconds_to_wait))
    {
        if (compare_write_to_read(sp) <= 0)
        {
            yella_unlock_mutex(sp->guard);
            return YELLA_TIMED_OUT;
        }
        msg_count = advance_to_next_unvisited(sp);
        *count = msg_count;
        if (msg_count > 0)
        {
            msg_off = ftell(sp->readf) - sizeof(msg_count);
            *parts = calloc(msg_count, sizeof(yella_message_part));
            for (i = 0; i < msg_count; i++)
            {
                if (fread(&msg_size, 1, sizeof(msg_size), sp->readf) != sizeof(msg_size))
                {
                    utf8 = yella_to_utf8(sp->read_file_name);
                    CHUCHO_C_ERROR_L(sp->lgr,
                                     "Unable to read the message size from %s",
                                     utf8);
                    free(utf8);
                    free(*parts);
                    *parts = NULL;
                    yella_unlock_mutex(sp->guard);
                    return YELLA_READ_ERROR;
                }
                (*parts)[i].size = msg_size;
                (*parts)[i].data = malloc(msg_size);
                if (fread((*parts)[i].data, 1, msg_size, sp->readf) != msg_size)
                {
                    utf8 = yella_to_utf8(sp->read_file_name);
                    CHUCHO_C_ERROR_L(sp->lgr,
                                     "Unable to read the message from %s",
                                     utf8);
                    free(utf8);
                    free((*parts)[i].data);
                    free(*parts);
                    *parts = NULL;
                    yella_unlock_mutex(sp->guard);
                    return YELLA_READ_ERROR;
                }
            }
            msg_count |= YELLA_VISITED_BIT;
            cur_off = ftell(sp->readf);
            fseek(sp->readf, msg_off, SEEK_SET);
            if (fwrite(&msg_count, 1, sizeof(msg_count), sp->readf) != sizeof(msg_count))
            {
                utf8 = yella_to_utf8(sp->read_file_name);
                CHUCHO_C_ERROR_L(sp->lgr,
                                 "Could not set the current event as visited (%s): %s",
                                 utf8,
                                 strerror(errno));
                free(utf8);
            }
            fseek(sp->readf, cur_off, SEEK_SET);
        }
        else
        {
            /*
             * The reader has caught up with the writer, which hasn't yet written
             * anything to a new file.
             */
            if (sp->read_pos.major_seq == sp->write_pos.major_seq &&
                sp->read_pos.minor_seq == sp->write_pos.minor_seq &&
                ftell(sp->readf) == ftell(sp->writef))
            {
                yella_unlock_mutex(sp->guard);
                return YELLA_TIMED_OUT;
            }
            else
            {
                CHUCHO_C_ERROR_L(sp->lgr,
                                 "Expected an event to read");
                yella_unlock_mutex(sp->guard);
                return YELLA_LOGIC_ERROR;
            }
        }
    }
    else
    {
        yella_unlock_mutex(sp->guard);
        return YELLA_TIMED_OUT;
    }
    ++sp->stats.events_read;
    yella_unlock_mutex(sp->guard);
    return yrc;
}

yella_rc spool_push(spool* sp, const yella_message_part* msgs, size_t count)
{
    uint16_t num;
    uint32_t len;
    size_t i;
    size_t num_written;
    size_t event_size = 0;

    yella_lock_mutex(sp->guard);
    if (ftell(sp->writef) >= sp->stats.max_partition_size)
    {
        if (!increment_write_spool_partition(sp))
        {
            yella_unlock_mutex(sp->guard);
            return YELLA_FILE_SYSTEM_ERROR;
        }
    }
    num = (uint16_t)count;
    assert(num <= YELLA_MAX_MSG_COUNT);
    num_written = fwrite(&num, 1, sizeof(num), sp->writef);
    if (num_written != sizeof(num))
    {
        CHUCHO_C_ERROR_L(sp->lgr,
                         "Error writing message count to spool file. The spooler cannot continue.");
        yella_unlock_mutex(sp->guard);
        return YELLA_WRITE_ERROR;
    }
    event_size += num_written;
    sp->stats.current_size += num_written;
    for (i = 0; i < count; i++)
    {
        len = msgs[i].size;
        num_written = fwrite(&len, 1, sizeof(len), sp->writef);
        if (num_written != sizeof(len))
        {
            CHUCHO_C_ERROR_L(sp->lgr,
                             "Error writing message length (%zu) to spool file. The spooler cannot continue.",
                             i);
            yella_unlock_mutex(sp->guard);
            return YELLA_WRITE_ERROR;
        }
        event_size += num_written;
        sp->stats.current_size += num_written;
        num_written = fwrite(msgs[i].data, 1, len, sp->writef);
        if (num_written != len)
        {
            CHUCHO_C_ERROR_L(sp->lgr,
                             "Error writing message data (%zu) to spool file. The spooler cannot continue.",
                             i);
            yella_unlock_mutex(sp->guard);
            return YELLA_WRITE_ERROR;
        }
        event_size += num_written;
        sp->stats.current_size += num_written;
    }
    fflush(sp->writef);
    ++sp->stats.events_written;
    if (sp->stats.current_size > sp->stats.largest_size)
        sp->stats.largest_size = sp->stats.current_size;
    if (sp->stats.largest_event_size < event_size)
        sp->stats.largest_event_size = event_size;
    if (sp->stats.smallest_event_size > event_size)
        sp->stats.smallest_event_size = event_size;
    sp->total_event_bytes_written += event_size;
    if (sp->stats.current_size >= sp->stats.max_partitions * sp->stats.max_partition_size)
        cull(sp);
    yella_signal_condition_variable(sp->was_written_cond);
    yella_unlock_mutex(sp->guard);
    return YELLA_NO_ERROR;
}
