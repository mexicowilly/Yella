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
            {
                yella_push_back_ptr_vector(paths, udsnew(cur_inc));
            }
            else
            {
                while (*spesh != YELLA_DIR_SEP[0] && spesh >= cur_inc)
                    --spesh;
                if (spesh >= cur_inc)
                    yella_push_back_ptr_vector(paths, udsnewlen(cur_inc, (spesh == cur_inc) ? 1 : spesh - cur_inc));
            }
        }
    }
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
    }
    return paths;
}

static void worker_callback(fsw_cevent const* const evts, const unsigned count, void* udata)
{
    event_source* esrc;
    unsigned i;
    UChar* utf16;
    const UChar* cfg;

    esrc = (event_source*)udata;
    for (i = 0; i < count; i++)
    {
        utf16 = yella_from_utf8(evts[i].path);
        cfg = event_source_file_name_matches_any(esrc, utf16);
        if (cfg != NULL)
            esrc->callback(cfg, utf16, esrc->callback_udata);
        free(utf16);
    }
}

static void worker_main(void* udata)
{
    fsw_start_monitor(((event_source_fswatch*)udata)->fsw);
}

static void update_specs(const event_source* const esrc)
{
    event_source_fswatch* esf;
    char* utf8;
    size_t i;
    yella_ptr_vector* paths;

    esf = (event_source_fswatch*)esrc->impl;
    if (fsw_is_running(esf->fsw))
        fsw_stop_monitor(esf->fsw);
    if (esf->worker != NULL)
    {
        yella_join_thread(esf->worker);
        fsw_destroy_session(esf->fsw);
        yella_destroy_thread(esf->worker);
    }
    esf->fsw = fsw_init_session(system_default_monitor_type);
    fsw_set_callback(esf->fsw, worker_callback, (void*)esrc);
    fsw_set_recursive(esf->fsw, true);
    fsw_set_latency(esf->fsw, *yella_settings_get_uint(u"file", u"fs-monitor-latency-seconds"));
    paths = get_paths(esrc);
    for (i = 0; i < yella_ptr_vector_size(paths); i++)
    {
        utf8 = yella_to_utf8(yella_ptr_vector_at(paths, i));
        fsw_add_path(esf->fsw, utf8);
        free(utf8);
    }
    yella_destroy_ptr_vector(paths);
    esf->worker = yella_create_thread(worker_main, esf);

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
    if (fsw_is_running(esf->fsw))
        fsw_stop_monitor(esf->fsw);
    yella_join_thread(esf->worker);
    fsw_destroy_session(esf->fsw);
    yella_destroy_thread(esf->worker);
    free(esf);
}

void init_event_source_impl(event_source* esrc)
{
    if (fsw_init_library() == FSW_OK)
    {
        esrc->impl = calloc(1, sizeof(event_source_fswatch));
        update_specs(esrc);
    }
}

void remove_event_source_impl_spec(event_source* esrc, const UChar* const config_name)
{
    update_specs(esrc);
}
