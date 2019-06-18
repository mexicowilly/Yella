#include "plugin/file/event_source.h"
#include "plugin/file/file_name_matcher.h"
#include "common/thread.h"
#include "common/settings.h"
#include "common/text_util.h"
#include "common/uds_util.h"
#include "common/file.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <libfswatch/c/libfswatch.h>
#include <unicode/ustring.h>
#include <stdlib.h>

typedef struct event_source_fswatch
{
    FSW_HANDLE fsw;
    yella_thread* worker;
} event_source_fswatch;

static int qsort_strcmp(const void* p1, const void* p2)
{
    const UChar* u1;
    const UChar* u2;

    u1 = *(const UChar**)p1;
    u2 = *(const UChar**)p2;
    return u_strcmp(u1, u2);
}

static yella_ptr_vector* get_paths(const event_source* const esrc)
{
    struct sglib_event_source_spec_iterator spec_itor;
    event_source_spec* cur_spec;
    size_t i;
    const UChar* cur_inc;
    const UChar* spesh;
    yella_ptr_vector* paths;
    uds log_msg;
    int32_t cur_len;
    char* utf8;
    yella_ptr_vector* uniq;

    paths = yella_create_uds_ptr_vector();
    for (cur_spec = sglib_event_source_spec_it_init(&spec_itor, esrc->specs);
         cur_spec != NULL;
         cur_spec = sglib_event_source_spec_it_next(&spec_itor))
    {
        for (i = 0; i < yella_ptr_vector_size(cur_spec->includes); i++)
        {
            cur_inc = yella_ptr_vector_at(cur_spec->includes, i);
            spesh = first_unescaped_special_char(cur_inc);
            if (spesh == NULL)
                spesh = u_strrchr(cur_inc, YELLA_DIR_SEP[0]);
            assert(spesh != NULL);
            while (*spesh != YELLA_DIR_SEP[0] && spesh >= cur_inc)
                --spesh;
            if (spesh >= cur_inc)
                yella_push_back_ptr_vector(paths, udsnewlen(cur_inc, (spesh == cur_inc) ? 1 : spesh - cur_inc));
        }
    }
    if (yella_ptr_vector_size(paths) > 0)
    {
        qsort(yella_ptr_vector_data(paths),
              yella_ptr_vector_size(paths),
              sizeof(UChar*),
              qsort_strcmp);
        uniq = yella_create_uds_ptr_vector();
        yella_push_back_ptr_vector(uniq, udsdup(yella_ptr_vector_at(paths, 0)));
        for (i = 1; i < yella_ptr_vector_size(paths); i++)
        {
            if (u_strcmp(yella_ptr_vector_at(paths, i), yella_ptr_vector_at(uniq, yella_ptr_vector_size(uniq) - 1)) != 0)
                yella_push_back_ptr_vector(uniq, udsdup(yella_ptr_vector_at(paths, i)));
        }
        yella_destroy_ptr_vector(paths);
        paths = uniq;
        if (chucho_logger_permits(esrc->lgr, CHUCHO_INFO))
        {
            log_msg = udscatprintf(udsempty(), u"Watching paths:%S", YELLA_NL);
            for (i = 0; i < yella_ptr_vector_size(paths); i++)
                log_msg = udscatprintf(log_msg, u"  '%S'%S", yella_ptr_vector_at(paths, i), YELLA_NL);
            cur_len = u_strlen(YELLA_NL);
            udsrange(log_msg, 0, -cur_len - 1);
            utf8 = yella_to_utf8(log_msg);
            CHUCHO_C_INFO(esrc->lgr, "%s", utf8);
            free(utf8);
            udsfree(log_msg);
        }
    }
    return paths;
}

static bool has_useful_flags(const fsw_cevent* const evt)
{
    unsigned i;

    for (i = 0; i < evt->flags_num; i++)
    {
        if (evt->flags[i] != NoOp && evt->flags[i] != PlatformSpecific)
            return true;
    }
    return false;
}

static void worker_callback(fsw_cevent const* const evts, const unsigned count, void* udata)
{
    event_source* esrc;
    unsigned i;
    UChar* utf16;
    const UChar* cfg;
    yella_ptr_vector* called;
    unsigned j;

    esrc = (event_source*)udata;
    called = yella_create_uds_ptr_vector();
    for (i = 0; i < count; i++)
    {
        if (has_useful_flags(&evts[i]))
        {
            utf16 = yella_from_utf8(evts[i].path);
            for (j = 0; j < yella_ptr_vector_size(called); j++)
            {
                if (u_strcmp(utf16, yella_ptr_vector_at(called, j)) == 0)
                    break;
            }
            if (j == yella_ptr_vector_size(called))
            {
                cfg = event_source_file_name_matches_any(esrc, utf16);
                if (cfg != NULL)
                    esrc->callback(cfg, utf16, esrc->callback_udata);
                yella_push_back_ptr_vector(called, udsnew(utf16));
            }
            free(utf16);
        }
    }
    yella_destroy_ptr_vector(called);
}

static void worker_main(void* udata)
{
    event_source* esrc;
    event_source_fswatch* esf;

    esrc = udata;
    CHUCHO_C_INFO(esrc->lgr, "Fswatch event source thread starting");
    esf = esrc->impl;
    fsw_start_monitor(esf->fsw);
    CHUCHO_C_INFO(esrc->lgr, "Fswatch event source thread ending");
}

static void start_monitor(const event_source* const esrc)
{
    event_source_fswatch* esf;
    yella_ptr_vector* paths;
    char* utf8;
    size_t i;

    esf = (event_source_fswatch*)esrc->impl;
    if (esf != NULL)
    {
        paths = get_paths(esrc);
        if (yella_ptr_vector_size(paths) > 0)
        {
            esf->fsw = fsw_init_session(system_default_monitor_type);
            fsw_set_callback(esf->fsw, worker_callback, (void*)esrc);
            fsw_set_recursive(esf->fsw, true);
            fsw_set_latency(esf->fsw, *yella_settings_get_uint(u"file", u"fs-monitor-latency-seconds"));
            /* fsw_set_verbose(true); */
            for (i = 0; i < yella_ptr_vector_size(paths); i++)
            {
                utf8 = yella_to_utf8(yella_ptr_vector_at(paths, i));
                fsw_add_path(esf->fsw, utf8);
                free(utf8);
            }
            esf->worker = yella_create_thread(worker_main, (void*)esrc);
            /* There is no way to synchronize the start of the monitor with libfswatch.
             * And if we don't let it start, then stopping it right away will hang the
             * join of the monitor thread. */
            yella_sleep_this_thread_milliseconds(100);
        }
        yella_destroy_ptr_vector(paths);
    }
}

static void stop_monitor(event_source_fswatch* esf)
{
    if (esf != NULL)
    {
        if (esf->fsw != NULL && fsw_is_running(esf->fsw))
            fsw_stop_monitor(esf->fsw);
        if (esf->worker != NULL)
        {
            yella_join_thread(esf->worker);
            yella_destroy_thread(esf->worker);
            esf->worker = NULL;
        }
        if (esf->fsw != NULL)
        {
            fsw_destroy_session(esf->fsw);
            esf->fsw = NULL;
        }
    }
}

static void update_specs(const event_source* const esrc)
{
    event_source_fswatch* esf;

    esf = (event_source_fswatch*)esrc->impl;
    stop_monitor(esf);
    start_monitor(esrc);
}

void add_or_replace_event_source_impl_spec(event_source* esrc, event_source_spec* spec)
{
    update_specs(esrc);
}

void clear_event_source_impl_specs(event_source* esrc)
{
    update_specs(esrc);
}

void destroy_event_source_impl(event_source* esrc)
{
    event_source_fswatch* esf;

    esf = esrc->impl;
    stop_monitor(esf);
    free(esf);
    esrc->impl = NULL;
}

void init_event_source_impl(event_source* esrc)
{
    if (fsw_init_library() == FSW_OK)
    {
        esrc->impl = calloc(1, sizeof(event_source_fswatch));
        start_monitor(esrc);
    }
}

void pause_event_source(event_source* esrc)
{
    yella_thread* pauser;
    event_source_fswatch* esf;

    esf = (event_source_fswatch*)esrc->impl;
    if (esf != NULL)
    {
        /* The monitor cannot be stopped in the event callback thread */
        pauser = yella_create_thread((yella_thread_func)stop_monitor, esf);
        yella_detach_thread(pauser);
        yella_destroy_thread(pauser);
    }
}

void remove_event_source_impl_spec(event_source* esrc, const UChar* const config_name)
{
    update_specs(esrc);
}

void resume_event_source(event_source* esrc)
{
    event_source_fswatch* esf;

    esf = (event_source_fswatch*)esrc->impl;
    if (esf->fsw == NULL)
    {
        assert(esf->worker == NULL);
        start_monitor(esrc);
    }
}
