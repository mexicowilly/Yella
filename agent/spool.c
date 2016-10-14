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
#include "common/log.h"
#include "common/thread.h"
#include "common/text_util.h"
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
    /* You don't own the router */
    yella_router* rtr;
    yella_thread* reader_thread;
    yella_mutex* guard;
    bool is_stopped;
    yella_condition_variable* was_written_cond;
    char* read_file_name;
    char* write_file_name;
    uintmax_t size;
    yella_spool_size_notification size_notification;
    void* size_notification_data;
    yella_mutex* size_guard;
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
    const char* dir;
    char* result;
    const char* fmt;
    size_t len;

    fmt = "%s%s%lu-%lu.yella.spool";
    dir = yella_settings_get_text("spool-dir");
    len = snprintf(NULL, 0, fmt, dir, YELLA_DIR_SEP, (unsigned long)major_seq, (unsigned long)minor_seq) + 1;
    result = malloc(len);
    snprintf(result, len, fmt, dir, YELLA_DIR_SEP, (unsigned long)major_seq, (unsigned long)minor_seq);
    return result;
}

static void add_to_spool_size(yella_spool* sp, intmax_t val)
{
    static double last_reported_pct = 0.0;
    double pct;
    const uint64_t* max_total;

    if (val != 0)
    {
        yella_lock_mutex(sp->size_guard);
        assert(val > 0 || -val >= sp->size);
        sp->size += val;
        max_total = yella_settings_get_uint("max-total-spool");
        assert(max_total != NULL);
        pct = sp->size * 100.0 / (double)*max_total;
        if (fabs(pct - last_reported_pct) >= 10.0)
        {
            last_reported_pct = trunc(pct / 10.0);
            CHUCHO_C_INFO(yella_logger("yella.spool"),
                          "The spool is at %0f%% of capacity (%ju bytes)",
                          last_reported_pct,
                          sp->size);
        }
        if (sp->size_notification != NULL)
            sp->size_notification(pct, sp->size_notification_data);
        yella_unlock_mutex(sp->size_guard);
    }
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
        CHUCHO_C_ERROR(yella_logger("yella.spool"),
                       "Error closing write stream: %s",
                       strerror(err));
    }
    add_to_spool_size(sp, yella_file_size(sp->write_file_name));
    free(sp->write_file_name);
    sp->write_file_name = spool_file_name(sp->write_pos.major_seq, ++sp->write_pos.minor_seq);
    sp->writef = fopen(sp->write_file_name, "wb");
    if (sp->writef == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR(yella_logger("yella.spool"),
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
        CHUCHO_C_ERROR(yella_logger("yella.spool"),
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
        CHUCHO_C_ERROR(yella_logger("yella.spool"),
                       "Unable to flush spool file %s",
                       fname);
        fclose(sp->writef);
        sp->writef = NULL;
        free(sp->write_file_name);
        sp->write_file_name = NULL;
        return false;
    }
    CHUCHO_C_TRACE(yella_logger("yella.spool"),
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
        CHUCHO_C_TRACE(yella_logger("yella.spool"),
                       "Closing read stream %s",
                       sp->read_file_name);
        free(sp->read_file_name);
        sp->read_file_name = NULL;
    }
    if (sp->readf != NULL && fclose(sp->readf) != 0)
    {
        err = errno;
        CHUCHO_C_WARN(yella_logger("yella.spool"),
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
            CHUCHO_C_WARN(yella_logger("yella.spool"),
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
        CHUCHO_C_ERROR(yella_logger("yella.spool"),
                       "Unable to open spool file %s for reading: %s",
                       candidate,
                       strerror(err));
        free(candidate);
        return false;
    }
    sp->read_file_name = candidate;
    CHUCHO_C_TRACE(yella_logger("yella.spool"),
                   "Opened %s for reading",
                   sp->read_file_name);
    fseek(sp->readf, sizeof(YELLA_SPOOL_ID), SEEK_SET);
    sp->read_pos.major_seq = min_major_seq;
    sp->read_pos.minor_seq = min_minor_seq;
    return true;
}

/**
 * Returns message count in the next unvisited entry and sets
 * the file pointer to the uint32_t that has the size of the
 * first message part.
 *
 * On error returns zero. If end-of-file, then feof(readf) will
 * indicate.
 */
static uint16_t advance_to_next_unvisited(yella_spool* sp)
{
    uint16_t msg_count;
    size_t num_read;
    uint32_t part_size;
    uint16_t i;
    int err;
    chucho_logger* lgr;
    size_t visited_count;
    long cur_write_pos;

    lgr = yella_logger("yella.spool");
    visited_count = 0;
    do
    {
        num_read = fread(&msg_count, 1, sizeof(msg_count), sp->readf);
        if (num_read == 0)
        {
            if (feof(sp->readf) != 0)
            {
                yella_lock_mutex(sp->guard);
                if (sp->read_pos.major_seq == sp->write_pos.major_seq &&
                    sp->read_pos.minor_seq == sp->write_pos.minor_seq)
                {
                    cur_write_pos = ftell(sp->writef);
                    CHUCHO_C_TRACE(lgr,
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
                    CHUCHO_C_TRACE(lgr,
                                   "Ready to read more data from file %s",
                                   sp->read_file_name);
                }
                else if (sp->read_pos.major_seq < sp->write_pos.major_seq ||
                         (sp->read_pos.major_seq == sp->write_pos.major_seq &&
                          sp->read_pos.minor_seq < sp->write_pos.minor_seq))
                {
                    /* advance to next */
                    yella_unlock_mutex(sp->guard);
                    CHUCHO_C_TRACE(lgr,
                                   "Done reading from %s. It is being removed.",
                                   sp->read_file_name);
                    if (fclose(sp->readf) != 0)
                    {
                        err = errno;
                        CHUCHO_C_WARN(lgr,
                                      "Error closing read stream %s: %s",
                                      sp->read_file_name,
                                      strerror(err));
                    }
                    sp->readf = NULL;
                    if (remove(sp->read_file_name) == 0)
                    {
                        add_to_spool_size(sp, -yella_file_size(sp->read_file_name));
                    }
                    else
                    {
                        err = errno;
                        CHUCHO_C_WARN(lgr,
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
                    CHUCHO_C_ERROR(lgr,
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
                CHUCHO_C_ERROR(lgr,
                               "Zero bytes were read from %s without reaching the end of file.",
                               sp->read_file_name);
                return 0;
            }
        }
        else if (num_read < sizeof(msg_count))
        {
            CHUCHO_C_ERROR(lgr,
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
                CHUCHO_C_TRACE(lgr,
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

typedef struct state_change_data
{
    yella_mutex* guard;
    yella_condition_variable* cond;
    yella_router_state st;
} state_change_data;

static void router_state_changed(yella_router_state st, void* data)
{
    state_change_data* ch_data;

    ch_data = (state_change_data*)data;
    yella_lock_mutex(ch_data->guard);
    ch_data->st = st;
    if (st == YELLA_ROUTER_CONNECTED)
        yella_signal_condition_variable(ch_data->cond);
    yella_unlock_mutex(ch_data->guard);
}

static void reader_thread_main(void* arg)
{
    yella_spool* sp;
    state_change_data ch_data;
    bool stopped;
    uint16_t msg_count;
    uint16_t i;
    yella_msg_part* msgs;
    size_t msgs_capacity;
    uint32_t msg_size;
    long start_pos;
    long end_pos;
    yella_sender* sndr;

    sp = (yella_spool*)arg;
    ch_data.guard = yella_create_mutex();
    ch_data.cond = yella_create_condition_variable();
    ch_data.st = yella_router_get_state(sp->rtr);
    yella_set_router_state_callback(sp->rtr, router_state_changed, &ch_data);
    sndr = yella_create_sender(sp->rtr);
    msgs_capacity = 0;
    msgs = NULL;
    while (true)
    {
        yella_lock_mutex(sp->guard);
        stopped = sp->is_stopped;
        yella_unlock_mutex(sp->guard);
        if (stopped)
            break;
        yella_lock_mutex(ch_data.guard);
        while (ch_data.st != YELLA_ROUTER_CONNECTED)
        {
            yella_wait_milliseconds_for_condition_variable(ch_data.cond, ch_data.guard, 250);
            yella_lock_mutex(sp->guard);
            stopped = sp->is_stopped;
            yella_unlock_mutex(sp->guard);
            if (stopped)
                break;
        }
        yella_unlock_mutex(ch_data.guard);
        yella_lock_mutex(sp->guard);
        stopped = sp->is_stopped;
        yella_unlock_mutex(sp->guard);
        if (stopped)
            break;
        msg_count = advance_to_next_unvisited(sp);
        if (msg_count == 0)
            break;
        if (msg_count > msgs_capacity)
        {
            msgs_capacity = msg_count;
            msgs = malloc(sizeof(yella_msg_part) * msgs_capacity);
        }
        start_pos = ftell(sp->readf) - sizeof(uint16_t);
        for (i = 0; i < msg_count; i++)
        {
            if (fread(&msg_size, 1, sizeof(msg_size), sp->readf) != sizeof(msg_size))
            {
                /* error */
            }
            msgs[i].size = msg_size;
            msgs[i].data = malloc(msg_size);
            if (fread(msgs[i].data, 1, msg_size, sp->readf) != msg_size)
            {
                /* error */
            }
        }
        if (yella_send(sndr, msgs, msg_count))
        {
            msg_count |= YELLA_VISITED_BIT;
            end_pos = ftell(sp->readf);
            fseek(sp->readf, start_pos, SEEK_SET);
            if (fwrite(&msg_count, 1, sizeof(msg_count), sp->readf) != sizeof(msg_count))
            {
                /* error */
            }
            fseek(sp->readf, end_pos, SEEK_SET);
        }
        else
        {
            /* error */
        }
    }
    free(msgs);
    yella_destroy_condition_variable(ch_data.cond);
    yella_destroy_mutex(ch_data.guard);
    yella_destroy_sender(sndr);
    CHUCHO_C_INFO(yella_logger("yella.spool"),
                  "The spool reader thread is exiting");
}

static bool init_reader(yella_spool* sp)
{
    bool result;

    sp->readf = NULL;
    sp->read_pos.major_seq = 0;
    sp->read_pos.minor_seq = 0;
    sp->read_file_name = NULL;
    result = increment_read_spool_partition(sp);
    if (result == true)
        sp->reader_thread = yella_create_thread(reader_thread_main, sp);
    return result;
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

yella_spool* yella_create_spool(const yella_saved_state* state, yella_router* rtr)
{
    yella_spool* sp;

    sp = malloc(sizeof(yella_spool));
    /* You don't own the router */
    sp->rtr = rtr;
    sp->guard = yella_create_mutex();
    sp->was_written_cond = yella_create_condition_variable();
    sp->is_stopped = false;
    sp->size = current_spool_size();
    sp->size_notification = NULL;
    sp->size_notification_data = NULL;
    sp->size_guard = yella_create_mutex();
    if (!init_writer(sp, state) || !init_reader(sp))
    {
        yella_destroy_mutex(sp->size_guard);
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
    yella_join_thread(sp->reader_thread);
    yella_destroy_thread(sp->reader_thread);
    fclose(sp->readf);
    fclose(sp->writef);
    yella_destroy_mutex(sp->guard);
    yella_destroy_condition_variable(sp->was_written_cond);
    free(sp->write_file_name);
    free(sp->read_file_name);
    yella_destroy_mutex(sp->size_guard);
    free(sp);
}

void yella_set_spool_size_notification(yella_spool* sp, yella_spool_size_notification sn, void* data)
{
    sp->size_notification = sn;
    sp->size_notification_data = data;
}

bool yella_write_spool(yella_spool* sp, yella_msg_part* msgs, size_t count)
{
    uint16_t num;
    uint32_t len;
    size_t i;
    size_t num_written;

    if (ftell(sp->writef) >= *yella_settings_get_uint("max-spool-partition"))
    {
        if (!increment_write_spool_partition(sp))
            return false;
    }
    num = count;
    assert(num <= YELLA_MAX_MSG_COUNT);
    yella_lock_mutex(sp->guard);
    num_written = fwrite(&num, 1, sizeof(num), sp->writef);
    if (num_written != sizeof(num))
    {
        CHUCHO_C_ERROR(yella_logger("yella.spool"),
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
            CHUCHO_C_ERROR(yella_logger("yella.spool"),
                           "Error writing message length (%zu) to spool file. The spooler cannot continue.",
                           i);
            yella_unlock_mutex(sp->guard);
            return false;
        }
        fwrite(msgs[i].data, 1, len, sp->writef);
        if (num_written != len)
        {
            CHUCHO_C_ERROR(yella_logger("yella.spool"),
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
