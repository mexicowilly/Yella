#include "plugin/plugin.h"
#include "common/thread.h"
#include "common/settings.h"
#include "common/macro_util.h"
#include "common/file.h"
#include "plugin/file/attribute.h"
#include <chucho/logger.h>

typedef struct file_plugin
{
    yella_plugin* desc;
    chucho_logger_t* lgr;
    yella_mutex* guard;
    yella_agent_api* agent_api;
} file_plugin;

file_plugin* fplg;

static yella_rc monitor_handler(const uint8_t* const msg, size_t sz, void* udata)
{
    return YELLA_NO_ERROR;
}

static void retrieve_file_settings(void)
{
    uds data_dir;

    yella_setting_desc descs[] =
    {
        { u"data-dir", YELLA_SETTING_VALUE_TEXT },
        { u"max-spool-dbs", YELLA_SETTING_VALUE_UINT }
    };

    data_dir = udsnew(yella_settings_get_text(u"agent", u"data-dir"));
    data_dir = udscat(data_dir, YELLA_DIR_SEP);
    data_dir = udscat(data_dir, u"file");
    yella_settings_set_text(u"file", u"data-dir", data_dir);
    yella_settings_set_uint(u"file", u"max-spool-dbs", 100);
    udsfree(data_dir);

    yella_retrieve_settings(u"file", descs, YELLA_ARRAY_SIZE(descs));
}

YELLA_EXPORT yella_plugin* plugin_start(const yella_agent_api* api, void* agnt)
{
    retrieve_file_settings();
    fplg = malloc(sizeof(file_plugin));
    fplg->lgr = chucho_get_logger("yella.file");
    fplg->guard = yella_create_mutex();
    fplg->desc = yella_create_plugin(u"file", u"1", fplg);
    yella_push_back_ptr_vector(fplg->desc->in_caps,
                               yella_create_plugin_in_cap(u"file.monitor_request", 1, monitor_handler, fplg));
    yella_push_back_ptr_vector(fplg->desc->out_caps,
                               yella_create_plugin_out_cap(u"file.change", 1));
    fplg->agent_api = yella_copy_agent_api(api);
    return yella_copy_plugin(fplg->desc);
}

YELLA_EXPORT yella_plugin* plugin_status(void* udata)
{
    yella_plugin* result;
    file_plugin* fplg;

    fplg = (file_plugin*)udata;
    yella_lock_mutex(fplg->guard);
    result = yella_copy_plugin(fplg->desc);
    yella_unlock_mutex(fplg->guard);
    return result;
}

YELLA_EXPORT yella_rc plugin_stop(void* udata)
{
    file_plugin* fplg;

    fplg = (file_plugin*)udata;
    yella_destroy_plugin(fplg->desc);
    yella_destroy_mutex(fplg->guard);
    yella_destroy_agent_api(fplg->agent_api);
    chucho_release_logger(fplg->lgr);
    free(fplg);
    return YELLA_NO_ERROR;
}
