#include "plugin/plugin.h"
#include "common/text_util.h"
#include <chucho/log.h>

yella_plugin* yella_copy_plugin(const yella_plugin* const plug)
{
    yella_plugin* result;
    int i;
    int j;

    result = malloc(sizeof(yella_plugin));
    result->name = yella_text_dup(plug->name);
    result->version = yella_text_dup(plug->version);
    result->in_cap_count = plug->in_cap_count;
    result->in_caps = malloc(plug->in_cap_count * sizeof(yella_plugin_in_cap));
    for (i = 0; i < plug->in_cap_count; i++)
    {
        result->in_caps[i].name = yella_text_dup(plug->in_caps[i].name);
        result->in_caps[i].version = plug->in_caps[i].version;
        result->in_caps[i].handler = plug->in_caps[i].handler;
        result->in_caps[i].config_count = plug->in_caps[i].config_count;
        if (plug->in_caps[i].config_count == 0)
            result->in_caps[i].configs = NULL;
        else
            result->in_caps[i].configs = malloc(plug->in_caps[i].config_count * sizeof(char*));
        for (j = 0; j < plug->in_caps[i].config_count; j++)
            result->in_caps[i].configs[j] = yella_text_dup(plug->in_caps[i].configs[j]);
    }
    result->out_cap_count = plug->out_cap_count;
    for (i = 0; i < plug->out_cap_count; i++)
    {
        result->out_caps[i].name = yella_text_dup(plug->out_caps[i].name);
        result->out_caps[i].version = plug->out_caps[i].version;
    }
    return result;
}

void yella_destroy_plugin(yella_plugin* plug)
{
    int i;
    int j;

    free(plug->name);
    free(plug->version);
    for (i = 0; i < plug->in_cap_count; i++)
    {
        free(plug->in_caps[i].name);
        for (j = 0; j < plug->in_caps[i].config_count; j++)
            free(plug->in_caps[i].configs[j]);
    }
    for (i = 0; i < plug->out_cap_count; i++)
        free(plug->out_caps[i].name);
    free(plug);
}

void yella_log_plugin_config(const char* const lgr, yella_fb_plugin_config_table_t cfg)
{
    CHUCHO_C_INFO(lgr,
                  "yella.fb.plugin.config: name = %s, action = %s",
                  yella_fb_plugin_config_name(cfg),
                  yella_fb_plugin_config_action_name(yella_fb_plugin_config_action(cfg)));
}
