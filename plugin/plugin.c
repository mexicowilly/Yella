#include "plugin/plugin.h"
#include "common/uds_util.h"
#include <chucho/log.h>

static void* in_cap_copier(void* p, void* udata)
{
    yella_plugin_in_cap* in;
    yella_plugin_in_cap* result;

    in = (yella_plugin_in_cap*)p;
    result = malloc(sizeof(yella_plugin_in_cap));
    result->name = udsnew(in->name);
    result->version = in->version;
    result->configs = yella_copy_ptr_vector(in->configs);
    return result;
}

static void in_cap_destructor(void* p, void* udata)
{
    yella_plugin_in_cap* in;

    in = (yella_plugin_in_cap*)p;
    udsfree(in->name);
    yella_destroy_ptr_vector(in->configs);
    free(in);
}

static void* out_cap_copier(void* p, void* udata)
{
    yella_plugin_out_cap* in;
    yella_plugin_out_cap* result;

    in = (yella_plugin_out_cap*)p;
    result = malloc(sizeof(yella_plugin_out_cap));
    result->name = udsnew(in->name);
    result->version = in->version;
    return result;
}

static void out_cap_destructor(void* p, void* udata)
{
    yella_plugin_out_cap* in;

    in = (yella_plugin_out_cap*)p;
    udsfree(in->name);
    free(in);
}

yella_plugin* yella_copy_plugin(const yella_plugin* const plug)
{
    yella_plugin* result;

    result = malloc(sizeof(yella_plugin));
    result->name = udsnew(plug->name);
    result->version = udsnew(plug->version);
    result->in_caps = yella_copy_ptr_vector(plug->in_caps);
    result->out_caps = yella_copy_ptr_vector(plug->out_caps);
    return result;
}

yella_plugin* yella_create_plugin(const UChar* const name, const UChar* const version)
{
    yella_plugin* plug;

    plug = calloc(1, sizeof(yella_plugin));
    plug->name = udsnew(name);
    plug->version = udsnew(version);
    plug->in_caps = yella_create_ptr_vector();
    yella_set_ptr_vector_copier(plug->in_caps, in_cap_copier, NULL);
    yella_set_ptr_vector_destructor(plug->in_caps, in_cap_destructor, NULL);
    plug->out_caps = yella_create_ptr_vector();
    yella_set_ptr_vector_copier(plug->out_caps, out_cap_copier, NULL);
    yella_set_ptr_vector_destructor(plug->out_caps, out_cap_destructor, NULL);
    return plug;
}

yella_plugin_in_cap* yella_create_plugin_in_cap(const UChar* const name, int version, yella_in_cap_handler handler)
{
    yella_plugin_in_cap* result;

    result = malloc(sizeof(yella_plugin_in_cap));
    result->name = udsnew(name);
    result->version = version;
    result->handler = handler;
    result->configs = yella_create_uds_ptr_vector();
    return result;
}

yella_plugin_out_cap* yella_create_plugin_out_cap(const UChar* const name, int version)
{
    yella_plugin_out_cap* result;

    result = malloc(sizeof(yella_plugin_out_cap));
    result->name = udsnew(name);
    result->version = version;
    return result;
}

void yella_destroy_plugin(yella_plugin* plug)
{
    udsfree(plug->name);
    udsfree(plug->version);
    yella_destroy_ptr_vector(plug->in_caps);
    yella_destroy_ptr_vector(plug->out_caps);
    free(plug);
}

void yella_log_plugin_config(chucho_logger_t* lgr, yella_fb_plugin_config_table_t cfg)
{
    CHUCHO_C_INFO_L(lgr,
                    "yella.fb.plugin.config: name = %s, action = %s",
                    yella_fb_plugin_config_name(cfg),
                    yella_fb_plugin_config_action_name(yella_fb_plugin_config_action(cfg)));
}
