#include "plugin/file/event_source.h"
#include "plugin/file/file_name_matcher.h"
#include "common/thread.h"
#include "common/settings.h"
#include "common/text_util.h"
#include "common/uds_util.h"
#include "common/file.h"
#include <libfswatch/c/libfswatch.h>
#include <unicode/ustring.h>
#include <stdlib.h>

typedef struct event_source_fswatch
{
    FSW_HANDLE fsw;
    yella_thread* worker;
    yella_mutex* guard;
    /* These are uds */
    yella_ptr_vector* paths;
} event_source_fswatch;

static void set_paths(const event_source* const esrc)
{
    event_source_fswatch* esf;
    struct sglib_event_source_spec_iterator spec_itor;
    event_source_spec* cur_spec;
    size_t i;
    const UChar* cur_inc;
    const UChar* spesh;

    esf = (event_source_fswatch*)esrc->impl;
    yella_destroy_ptr_vector(esf->paths);
    esf->paths = yella_create_uds_ptr_vector();
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
                yella_push_back_ptr_vector(esf->paths, udsnew(cur_inc));
            }
            else
            {
                while (*spesh != YELLA_DIR_SEP[0])
                    --spesh;
                yella_push_back_ptr_vector(esf->paths, udsnewlen(cur_inc, spesh - cur_inc));
            }
        }
    }
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
    }
}

static void worker_main(void* udata)
{
    fsw_start_monitor(((event_source_fswatch*)udata)->fsw);
}

void clear_event_source_impl_specs(event_source* esrc)
{
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
    yella_destroy_mutex(esf->guard);
    free(esf);
}

void init_event_source_impl(event_source* esrc)
{
    event_source_fswatch* esf;

    if (fsw_init_library() == FSW_OK)
    {
        esrc->impl = malloc(sizeof(event_source_fswatch));
        esf = esrc->impl;
        esf->fsw = fsw_init_session(system_default_monitor_type);
        fsw_set_callback(esf->fsw, worker_callback, esrc);
        fsw_set_recursive(esf->fsw, true);
        fsw_set_latency(esf->fsw, *yella_settings_get_uint(u"file", u"fs-monitor-latency-seconds"));
        esf->guard = yella_create_mutex();
        esf->worker = yella_create_thread(worker_main, esf);
    }
}

void remove_event_source_impl_spec(event_source* esrc, const UChar* const config_name)
{
}
