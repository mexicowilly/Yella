#include "plugin/plugin.h"
#include "common/thread.h"
#include "common/settings.h"
#include "common/macro_util.h"
#include "common/file.h"
#include "common/sglib.h"
#include "common/uds_util.h"
#include "common/text_util.h"
#include "plugin/file/job_queue.h"
#include "plugin/file/event_source.h"
#include "file_reader.h"
#include "file_config_builder.h"
#include <chucho/logger.h>
#include <unicode/ustring.h>
#include <stdio.h>
#include <errno.h>

typedef struct config_node
{
    uds name;
    uds topic;
    yella_ptr_vector* includes;
    yella_ptr_vector* excludes;
    attribute_type* attr_types;
    size_t attr_type_count;
    char color;
    struct config_node* left;
    struct config_node* right;
} config_node;

typedef struct file_plugin
{
    yella_plugin* desc;
    chucho_logger_t* lgr;
    yella_mutex* guard;
    yella_reader_writer_lock* config_guard;
    yella_agent_api* agent_api;
    job_queue* jq;
    config_node* configs;
    event_source* esrc;
    void* agent;
} file_plugin;

#define CONFIG_MAP_COMPARATOR(lhs, rhs) (u_strcmp(lhs->name, rhs->name))

SGLIB_DEFINE_RBTREE_PROTOTYPES(config_node, left, right, color, CONFIG_MAP_COMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(config_node, left, right, color, CONFIG_MAP_COMPARATOR);

static void destroy_config_node(config_node* cn)
{
    if (cn != NULL)
    {
        udsfree(cn->name);
        udsfree(cn->topic);
        yella_destroy_ptr_vector(cn->includes);
        yella_destroy_ptr_vector(cn->excludes);
        free(cn->attr_types);
        free(cn);
    }
}

static void event_received(const UChar* const config_name, const UChar* const fname, void* udata)
{
    file_plugin* fplg;
    config_node to_find;
    config_node* found;
    job* jb;
    char* cutf8;
    char* futf8;

    fplg = (file_plugin*)udata;
    to_find.name = (uds)config_name;
    yella_read_lock_reader_writer_lock(fplg->config_guard);
    found = sglib_config_node_find_member(fplg->configs, &to_find);
    if (found != NULL)
    {
        jb = create_job(config_name, fplg->agent_api, found->topic, fplg->agent);
        yella_push_back_ptr_vector(jb->includes, udsnew(fname));
        jb->attr_type_count = found->attr_type_count;
        jb->attr_types = malloc(sizeof(attribute_type) * jb->attr_type_count);
        memcpy(jb->attr_types, found->attr_types, sizeof(attribute_type) * jb->attr_type_count);
        yella_unlock_reader_writer_lock(fplg->config_guard);
        push_job_queue(fplg->jq, jb);
        if (chucho_logger_permits(fplg->lgr, CHUCHO_TRACE))
        {
            cutf8 = yella_to_utf8(config_name);
            futf8 = yella_to_utf8(fname);
            CHUCHO_C_TRACE(fplg->lgr, "Submitted job for config '%s': '%s'", cutf8, futf8);
            free(futf8);
            free(cutf8);
        }
    }
    else
    {
        yella_unlock_reader_writer_lock(fplg->config_guard);
        if (chucho_logger_permits(fplg->lgr, CHUCHO_WARN))
        {
            cutf8 = yella_to_utf8(config_name);
            futf8 = yella_to_utf8(fname);
            CHUCHO_C_WARN(fplg->lgr, "Unable to find config named '%s' for file '%s'", cutf8, futf8);
            free(futf8);
            free(cutf8);
        }
    }
}

static attribute_type fb_to_attribute_type(uint16_t fb)
{
    attribute_type result;

    switch (fb)
    {
    case yella_fb_file_attr_type_FILE_TYPE:
        result = ATTR_TYPE_FILE_TYPE;
        break;
    case yella_fb_file_attr_type_SHA_256:
        result = ATTR_TYPE_SHA256;
        break;
    }
    return result;
}

static uint16_t attribute_type_to_fb(attribute_type atp)
{
    uint16_t result;

    switch (atp)
    {
    case ATTR_TYPE_FILE_TYPE:
        result = yella_fb_file_attr_type_FILE_TYPE;
        break;
    case ATTR_TYPE_SHA256:
        result = yella_fb_file_attr_type_SHA_256;
        break;
    }
    return result;
}

static yella_rc monitor_handler(const uint8_t* const msg, size_t sz, void* udata)
{
    yella_fb_file_monitor_request_table_t tbl;
    yella_fb_plugin_config_table_t fb_cfg;
    yella_fb_plugin_config_action_enum_t act;
    yella_plugin_in_cap* mon;
    file_plugin* fplg;
    config_node* cfg;
    flatbuffers_string_vec_t svec;
    size_t i;
    flatbuffers_uint16_vec_t uvec;
    uint16_t atval;
    bool is_empty;
    size_t erase_idx;
    config_node* removed_cfg;
    char* utf8;
    event_source_spec* espec;

    fplg = (file_plugin*)udata;
    tbl = yella_fb_file_monitor_request_as_root(msg);
    if (!yella_fb_file_monitor_request_config_is_present(tbl))
    {
        CHUCHO_C_ERROR(fplg->lgr, "monitor_request: config field is missing");
        return YELLA_INVALID_FORMAT;
    }
    fb_cfg = yella_fb_file_monitor_request_config(tbl);
    if (!yella_fb_plugin_config_name_is_present(fb_cfg))
    {
        CHUCHO_C_ERROR(fplg->lgr, "monitor_request: config.name field is missing");
        return YELLA_INVALID_FORMAT;
    }
    CHUCHO_C_INFO(fplg->lgr, "Received monitor request %s", yella_fb_plugin_config_name(fb_cfg));
    cfg = malloc(sizeof(config_node));
    cfg->name = udsnew(yella_from_utf8(yella_fb_plugin_config_name(fb_cfg)));
    if (yella_fb_plugin_config_topic_is_present(fb_cfg))
        cfg->topic = udsnew(yella_from_utf8(yella_fb_plugin_config_topic(fb_cfg)));
    else
        cfg->topic = NULL;
    cfg->includes = yella_create_uds_ptr_vector();
    if (yella_fb_file_monitor_request_includes_is_present(tbl))
    {
        svec = yella_fb_file_monitor_request_includes(tbl);
        is_empty = flatbuffers_string_vec_len(svec) == 0;
        for (i = 0; i < flatbuffers_string_vec_len(svec); i++)
            yella_push_back_ptr_vector(cfg->includes, yella_from_utf8(flatbuffers_string_vec_at(svec, i)));
    }
    else
    {
        is_empty = true;
    }
    if (yella_fb_file_monitor_request_excludes_is_present(tbl))
    {
        cfg->excludes = yella_create_uds_ptr_vector();
        svec = yella_fb_file_monitor_request_excludes(tbl);
        for (i = 0; i < flatbuffers_string_vec_len(svec); i++)
            yella_push_back_ptr_vector(cfg->excludes, yella_from_utf8(flatbuffers_string_vec_at(svec, i)));
    }
    if (yella_fb_file_monitor_request_attr_types_is_present(tbl))
    {
        uvec = yella_fb_file_monitor_request_attr_types(tbl);
        cfg->attr_type_count = flatbuffers_uint16_vec_len(uvec);
        if (cfg->attr_type_count == 0)
        {
            cfg->attr_types = NULL;
        }
        else
        {
            cfg->attr_types = malloc(sizeof(attribute_type) * cfg->attr_type_count);
            for (i = 0; i < cfg->attr_type_count; i++)
            {
                atval = flatbuffers_uint16_vec_at(uvec, i);
                if (!yella_fb_file_attr_type_is_known_value(atval))
                {
                    CHUCHO_C_ERROR(fplg->lgr,
                                   "Invalid attriubte type value in config %s: %hu",
                                   yella_fb_plugin_config_name(fb_cfg),
                                   atval);
                }
                cfg->attr_types[i] = fb_to_attribute_type(atval);
            }
        }
    }
    else
    {
        cfg->attr_types = NULL;
        cfg->attr_type_count = 0;
    }
    act = yella_fb_plugin_config_action(fb_cfg);
    yella_write_lock_reader_writer_lock(fplg->config_guard);
    assert(yella_ptr_vector_size(fplg->desc->in_caps) == 1);
    mon = yella_ptr_vector_at(fplg->desc->in_caps, 0);
    assert(u_strcmp(mon->name, u"file.monitor_request") == 0);
    if (act == yella_fb_plugin_config_action_REPLACE_ALL)
        yella_clear_ptr_vector(mon->configs);
    for (i = 0; i < yella_ptr_vector_size(mon->configs); i++)
    {
        if (u_strcmp(yella_ptr_vector_at(mon->configs, i), cfg->name) == 0)
        {
            if (is_empty)
                erase_idx = i;
            break;
        }
    }
    if (is_empty)
        yella_erase_ptr_vector_at(mon->configs, erase_idx);
    else if (i == yella_ptr_vector_size(mon->configs))
        yella_push_back_ptr_vector(mon->configs, udsdup(cfg->name));
    sglib_config_node_delete_if_member(&fplg->configs, cfg, &removed_cfg);
    destroy_config_node(removed_cfg);
    utf8 = yella_to_utf8(cfg->name);
    if (is_empty)
    {
        remove_event_source_spec(fplg->esrc, cfg->name);
        destroy_config_node(cfg);
        CHUCHO_C_INFO(fplg->lgr, "Removed config '%s'", utf8);
    }
    else
    {
        sglib_config_node_add(&fplg->configs, cfg);
        espec = malloc(sizeof(event_source_spec));
        espec->name = udsdup(cfg->name);
        espec->includes = yella_copy_ptr_vector(cfg->includes);
        espec->excludes = yella_copy_ptr_vector(cfg->excludes);
        add_or_replace_event_source_spec(fplg->esrc, espec);
        CHUCHO_C_INFO(fplg->lgr, "Added config '%s'", utf8);
    }
    free(utf8);
    yella_unlock_reader_writer_lock(fplg->config_guard);
    return YELLA_NO_ERROR;
}

static void retrieve_file_settings(void)
{
    uds data_dir;

    yella_setting_desc descs[] =
    {
        { u"data-dir", YELLA_SETTING_VALUE_TEXT },
        { u"max-spool-dbs", YELLA_SETTING_VALUE_UINT },
        { u"max-events-in-cache", YELLA_SETTING_VALUE_UINT },
        { u"fs-monitor-latency-seconds", YELLA_SETTING_VALUE_UINT }
    };

    data_dir = udscatprintf(udsempty(), u"%S%Sfile", yella_settings_get_text(u"agent", u"data-dir"), YELLA_DIR_SEP);
    yella_settings_set_text(u"file", u"data-dir", data_dir);
    udsfree(data_dir);
    yella_settings_set_uint(u"file", u"max-spool-dbs", 100);
    yella_settings_set_uint(u"file", u"max-events-in-cache", 5000000);
    yella_settings_set_uint(u"file", u"fs-monitor-latency-seconds", 5);

    yella_retrieve_settings(u"file", descs, YELLA_ARRAY_SIZE(descs));
}

static void save_configs(const file_plugin* const fplg)
{
    uds fname;
    flatcc_builder_t bld;
    struct sglib_config_node_iterator itor;
    config_node* cur;
    char* utf8;
    size_t i;
    uint16_t fb_attr_type;
    uint8_t* ser;
    size_t ser_size;
    FILE* f;
    size_t num_written;

    flatcc_builder_init(&bld);
    yella_fb_file_configs_start_as_root(&bld);
    yella_fb_file_configs_cfgs_start(&bld);
    for (cur = sglib_config_node_it_init(&itor, fplg->configs);
         cur != NULL;
         cur = sglib_config_node_it_next(&itor))
    {
        yella_fb_file_config_start(&bld);
        utf8 = yella_to_utf8(cur->name);
        yella_fb_file_config_name_create_str(&bld, utf8);
        free(utf8);
        utf8 = yella_to_utf8(cur->topic);
        yella_fb_file_config_topic_create_str(&bld, utf8);
        free(utf8);
        yella_fb_file_config_includes_start(&bld);
        for (i = 0; i < yella_ptr_vector_size(cur->includes); i++)
        {
            utf8 = yella_to_utf8(yella_ptr_vector_at(cur->includes, i));
            yella_fb_file_config_includes_push_create_str(&bld, utf8);
            free(utf8);
        }
        yella_fb_file_config_includes_add(&bld, yella_fb_file_config_includes_end(&bld));
        yella_fb_file_config_excludes_start(&bld);
        for (i = 0; i < yella_ptr_vector_size(cur->excludes); i++)
        {
            utf8 = yella_to_utf8(yella_ptr_vector_at(cur->excludes, i));
            yella_fb_file_config_excludes_push_create_str(&bld, utf8);
            free(utf8);
        }
        yella_fb_file_config_excludes_add(&bld, yella_fb_file_config_excludes_end(&bld));
        yella_fb_file_config_attribute_types_start(&bld);
        for (i = 0; i < cur->attr_type_count; i++)
        {
            fb_attr_type = attribute_type_to_fb(cur->attr_types[i]);
            yella_fb_file_config_attribute_types_push(&bld, &fb_attr_type);
        }
        yella_fb_file_config_attribute_types_add(&bld, yella_fb_file_config_attribute_types_end(&bld));
        yella_fb_file_configs_cfgs_push(&bld, yella_fb_file_config_end(&bld));
    }
    yella_fb_file_configs_cfgs_end(&bld);
    yella_fb_file_configs_end_as_root(&bld);
    ser = flatcc_builder_finalize_buffer(&bld, &ser_size);
    fname = udscatprintf(udsempty(), u"%S%Sconfigs.flatb", yella_settings_get_text(u"file", u"data-dir"), YELLA_DIR_SEP);
    utf8 = yella_to_utf8(fname);
    udsfree(fname);
    f = fopen(utf8, "w");
    if (f == NULL)
    {
        CHUCHO_C_ERROR(fplg->lgr, "Unable to open '%s' for writing: %s", utf8, strerror(errno));
    }
    else
    {
        num_written = fwrite(ser, 1, ser_size, f);
        fclose(f);
        if (num_written != ser_size)
            CHUCHO_C_ERROR_L(fplg->lgr, "Could not write to '%s'", utf8);
    }
    free(utf8);
    free(ser);
}

YELLA_EXPORT yella_plugin* plugin_start(const yella_agent_api* api, void* agnt)
{
    file_plugin* fplg;

    retrieve_file_settings();
    fplg = malloc(sizeof(file_plugin));
    fplg->lgr = chucho_get_logger("yella.file");
    fplg->guard = yella_create_mutex();
    fplg->config_guard = yella_create_reader_writer_lock();
    fplg->desc = yella_create_plugin(u"file", u"1", fplg);
    yella_push_back_ptr_vector(fplg->desc->in_caps,
                               yella_create_plugin_in_cap(u"file.monitor_request", 1, monitor_handler, fplg));
    yella_push_back_ptr_vector(fplg->desc->out_caps,
                               yella_create_plugin_out_cap(u"file.change", 1));
    fplg->agent_api = yella_copy_agent_api(api);
    fplg->jq = create_job_queue();
    fplg->configs = NULL;
    fplg->esrc = create_event_source(event_received, fplg);
    fplg->agent = agnt;
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
    struct sglib_config_node_iterator itor;
    config_node* cur;

    fplg = (file_plugin*)udata;
    destroy_event_source(fplg->esrc);
    for (cur = sglib_config_node_it_init(&itor, fplg->configs);
         cur != NULL;
         cur = sglib_config_node_it_next(&itor))
    {
        destroy_config_node(cur);
    }
    destroy_job_queue(fplg->jq);
    yella_destroy_plugin(fplg->desc);
    yella_destroy_mutex(fplg->guard);
    yella_destroy_reader_writer_lock(fplg->config_guard);
    yella_destroy_agent_api(fplg->agent_api);
    chucho_release_logger(fplg->lgr);
    free(fplg);
    return YELLA_NO_ERROR;
}
