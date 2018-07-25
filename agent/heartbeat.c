#include "agent/heartbeat.h"
#include "plugin/plugin.h"
#include "heartbeat_builder.h"
#include <time.h>
#include <stdbool.h>

uint8_t* create_heartbeat(const char* id, const yella_ptr_vector* plugins, size_t* sz)
{
    flatcc_builder_t bld;
    int i;
    int j;
    int k;
    yella_plugin* plg;
    yella_plugin_in_cap* in_cap;
    yella_plugin_out_cap* out_cap;
    uint8_t* result;
    bool has_caps;
    bool has_configs;

    flatcc_builder_init(&bld);
    yella_fb_heartbeat_start_as_root(&bld);
    yella_fb_heartbeat_id_create_str(&bld, id);
    yella_fb_heartbeat_seconds_since_epoch_add(&bld, time(NULL));
    has_caps = false;
    for (i = 0; i < yella_ptr_vector_size(plugins); i++)
    {
        plg = (yella_plugin*)yella_ptr_vector_at(plugins, i);
        for (j = 0; j < yella_ptr_vector_size(plg->in_caps); j++)
        {
            if (!has_caps)
            {
                yella_fb_heartbeat_in_capabilities_start(&bld);
                has_caps = true;
            }
            in_cap = (yella_plugin_in_cap*)yella_ptr_vector_at(plg->in_caps, j);
            yella_fb_capability_start(&bld);
            yella_fb_capability_name_create_str(&bld, in_cap->name);
            yella_fb_capability_version_add(&bld, in_cap->version);
            has_configs = false;
            for (k = 0; k < yella_ptr_vector_size(in_cap->configs); k++)
            {
                if (!has_configs)
                {
                    yella_fb_capability_configurations_start(&bld);
                    has_configs = true;
                }
                yella_fb_capability_configurations_push_create_str(&bld,
                                                                   (const char*)yella_ptr_vector_at(in_cap->configs, k));
            }
            if (has_configs)
            {
                yella_fb_capability_configurations_add(&bld,
                                                       yella_fb_capability_configurations_end(&bld));
            }
            yella_fb_heartbeat_in_capabilities_push(&bld,
                                                    yella_fb_capability_end(&bld));
        }
    }
    if (has_caps)
    {
        yella_fb_heartbeat_in_capabilities_add(&bld, yella_fb_heartbeat_in_capabilities_end(&bld));
        has_caps = false;
    }
    for (i = 0; i < yella_ptr_vector_size(plugins); i++)
    {
        plg = (yella_plugin*) yella_ptr_vector_at(plugins, i);
        for (j = 0; j < yella_ptr_vector_size(plg->out_caps); j++)
        {
            if (!has_caps)
            {
                yella_fb_heartbeat_out_capabilities_start(&bld);
                has_caps = true;
            }
            out_cap = (yella_plugin_out_cap*)yella_ptr_vector_at(plg->out_caps, j);
            yella_fb_capability_start(&bld);
            yella_fb_capability_name_create_str(&bld, out_cap->name);
            yella_fb_capability_version_add(&bld, out_cap->version);
            yella_fb_heartbeat_out_capabilities_push(&bld, yella_fb_capability_end(&bld));
        }
    }
    if (has_caps)
        yella_fb_heartbeat_out_capabilities_add(&bld, yella_fb_heartbeat_out_capabilities_end(&bld));
    yella_fb_heartbeat_end_as_root(&bld);
    result = flatcc_builder_finalize_buffer(&bld, sz);
    flatcc_builder_clear(&bld);
    return result;
}

