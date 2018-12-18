#include "plugin/plugin.h"
#include "common/thread.h"
#include "common/settings.h"
#include "common/macro_util.h"
#include "common/file.h"
#include "common/sglib.h"
#include "plugin/file/job_queue.h"
#include "file_reader.h"
#include <chucho/logger.h>
#include <unicode/ustring.h>

typedef struct config_map
{
    uds name;
    yella_ptr_vector* includes;
    yella_ptr_vector* excludes;
    attribute_type* attr_types;
    size_t attr_type_count;
    char color;
    struct config_map* left;
    struct config_map* right;
} config_map;

typedef struct file_plugin
{
    yella_plugin* desc;
    chucho_logger_t* lgr;
    yella_mutex* guard;
    yella_agent_api* agent_api;
    job_queue* jq;
    config_map* configs;
} file_plugin;

#define CONFIG_MAP_COMPARATOR(lhs, rhs) (u_strcmp(lhs->name, rhs->name))

SGLIB_DEFINE_RBTREE_PROTOTYPES(config_map, left, right, color, CONFIG_MAP_COMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(config_map, left, right, color, CONFIG_MAP_COMPARATOR);

static yella_rc monitor_handler(const uint8_t* const msg, size_t sz, void* udata)
{
    yella_fb_file_monitor_request_table_t tbl;
    yella_fb_plugin_config_table_t cfg;
    yella_fb_plugin_config_action_enum_t act;
    yella_plugin_in_cap* mon;
    file_plugin* fplg;

    fplg = (file_plugin*)udata;
    tbl = yella_fb_file_monitor_request_as_root(msg);
    cfg = yella_fb_file_monitor_request_config(tbl);
    act = yella_fb_plugin_config_action(cfg);
    yella_lock_mutex(fplg->guard);
    assert(yella_ptr_vector_size(fplg->desc->in_caps) == 1);
    mon = yella_ptr_vector_at(fplg->desc->in_caps, 0);
    assert(u_strcmp(mon->name, u"file.monitor_request") == 0);
    yella_unlock_mutex(fplg->guard);
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
    file_plugin* fplg;

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
    fplg->jq = create_job_queue();
    fplg->configs = NULL;
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
    struct sglib_config_map_iterator itor;
    config_map* cur;

    fplg = (file_plugin*)udata;
    for (cur = sglib_config_map_it_init(&itor, fplg->configs);
         cur != NULL;
         cur = sglib_config_map_it_next(&itor))
    {
        udsfree(cur->name);
        free(cur);
    }
    destroy_job_queue(fplg->jq);
    yella_destroy_plugin(fplg->desc);
    yella_destroy_mutex(fplg->guard);
    yella_destroy_agent_api(fplg->agent_api);
    chucho_release_logger(fplg->lgr);
    free(fplg);
    return YELLA_NO_ERROR;
}
