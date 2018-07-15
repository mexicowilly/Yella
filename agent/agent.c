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

#include "agent/agent.h"
#include "agent/router.h"
#include "agent/yella_uuid.h"
#include "agent/saved_state.h"
#include "agent/spool.h"
#include "agent/runtime_link.h"
#include "common/settings.h"
#include "common/message_header.h"
#include "common/text_util.h"
#include "common/compression.h"
#include "common/message_part.h"
#include "common/file.h"
#include "common/thread.h"
#include "common/macro_util.h"
#include "plugin/plugin.h"
#include "heartbeat_builder.h"
#include <lz4.h>
#include <stdlib.h>
#include <chucho/log.h>
#include <stdatomic.h>

static const uint64_t YELLA_MEGABYTE = 1024 * 1024;
static const uint64_t YELLA_GIGABYTE = 1024 * 1024 * 1024;

typedef struct plugin_api
{
    char* name;
    void* shared_object;
    yella_plugin_start_func start_func;
    yella_plugin_status_func status_func;
    yella_plugin_stop_func stop_func;
} plugin_api;

typedef struct yella_agent
{
    yella_saved_state* state;
    yella_router* router;
    yella_spool* spool;
    yella_ptr_vector* plugins;
    yella_thread* heartbeat;
    atomic_bool should_stop;
} yella_agent;

static void plugin_dtor(void* plg, void* udata)
{
    yella_destroy_plugin((yella_plugin*)plg);
}

static void heartbeat_thr(void* udata)
{
    time_t next;
    size_t to_wait = *yella_settings_get_uint("agent", "hearbeat-seconds");
    yella_agent* ag = (yella_agent*)udata;
    flatcc_builder_t bld;
    int i;
    int j;
    int k;
    plugin_api* api;
    yella_plugin* plg;
    flatbuffers_string_vec_t configs;
    yella_ptr_vector* plugins;
    yella_plugin_in_cap* in_cap;
    yella_plugin_out_cap* out_cap;
    yella_sender* sndr;
    yella_message_part parts[2];
    yella_message_header* mhdr;
    uint32_t minor_seq;

    minor_seq = 0;
    sndr = yella_create_sender(ag->router);
    flatcc_builder_init(&bld);
    next = time(NULL) + to_wait;
    do
    {
        while (time(NULL) < next)
        {
            yella_sleep_this_thread(1000);
            if (ag->should_stop)
                break;
        }
        if (ag->should_stop)
            break;
        if (yella_router_get_state(ag->router) == YELLA_ROUTER_CONNECTED)
        {
            plugins = yella_create_ptr_vector();
            yella_set_ptr_vector_destructor(plugins, plugin_dtor, NULL);
            yella_fb_heartbeat_start_as_root(&bld);
            yella_fb_heartbeat_id_create_str(&bld, ag->state->id->text);
            yella_fb_heartbeat_seconds_since_epoch_add(&bld, time(NULL));
            for (i = 0; i < yella_ptr_vector_size(ag->plugins); i++)
            {
                api = (plugin_api *) yella_ptr_vector_at(ag->plugins, i);
                yella_push_back_ptr_vector(plugins, api->status_func());
            }
            yella_fb_heartbeat_in_capabilities_start(&bld);
            for (i = 0; i < yella_ptr_vector_size(plugins); i++)
            {
                plg = (yella_plugin *) yella_ptr_vector_at(plugins, i);
                for (j = 0; j < yella_ptr_vector_size(plg->in_caps); j++)
                {
                    in_cap = (yella_plugin_in_cap *) yella_ptr_vector_at(plg->in_caps, j);
                    yella_fb_capability_start(&bld);
                    yella_fb_capability_name_create_str(&bld, in_cap->name);
                    yella_fb_capability_version_add(&bld, in_cap->version);
                    yella_fb_capability_configurations_start(&bld);
                    for (k = 0; k < yella_ptr_vector_size(in_cap->configs); k++)
                    {
                        yella_fb_capability_configurations_push_create_str(&bld,
                                                                           (const char *) yella_ptr_vector_at(
                                                                               in_cap->configs, k));
                    }
                    yella_fb_capability_configurations_add(&bld,
                                                           yella_fb_capability_configurations_end(&bld));
                    yella_fb_heartbeat_in_capabilities_push(&bld,
                                                            yella_fb_capability_end(&bld));
                }
            }
            yella_fb_heartbeat_in_capabilities_add(&bld, yella_fb_heartbeat_in_capabilities_end(&bld));
            yella_fb_heartbeat_out_capabilities_start(&bld);
            for (i = 0; i < yella_ptr_vector_size(plugins); i++)
            {
                plg = (yella_plugin *) yella_ptr_vector_at(plugins, i);
                for (j = 0; j < yella_ptr_vector_size(plg->out_caps); j++)
                {
                    out_cap = (yella_plugin_out_cap *) yella_ptr_vector_at(plg->out_caps, j);
                    yella_fb_capability_start(&bld);
                    yella_fb_capability_name_create_str(&bld, out_cap->name);
                    yella_fb_capability_version_add(&bld, out_cap->version);
                    yella_fb_heartbeat_out_capabilities_push(&bld, yella_fb_capability_end(&bld));
                }
            }
            yella_destroy_ptr_vector(plugins);
            yella_fb_heartbeat_out_capabilities_add(&bld, yella_fb_heartbeat_out_capabilities_end(&bld));
            yella_fb_heartbeat_end_as_root(&bld);
            parts[1].data = flatcc_builder_finalize_buffer(&bld, &parts[1].size);
            flatcc_builder_reset(&bld);
            mhdr = yella_create_mhdr();
            mhdr->time = time(NULL);
            mhdr->sender = yella_text_dup(ag->state->id->text);
            mhdr->type = yella_text_dup("yella.heartbeat");
            mhdr->cmp = YELLA_COMPRESSION_NONE;
            mhdr->seq.major = ag->state->boot_count;
            mhdr->seq.minor = ++minor_seq;
            parts[0].data = yella_pack_mhdr(mhdr, &parts[0].size);
            if (yella_send(sndr, parts, 2))
                CHUCHO_C_INFO("yella.agent", "Sent heartbeat");
            else
                CHUCHO_C_INFO("yella.agent", "Error sending heartbeat");
        }
        else
        {
            CHUCHO_C_INFO("yella.agent",
                          "Not sending heartbeat because there is no router connection");
        }
        next = time(NULL) + to_wait;
    } while (true);
    flatcc_builder_clear(&bld);
    yella_destroy_sender(sndr);
}

static void send_plugin_message(void* agent,
                                yella_message_header* mhdr,
                                const uint8_t* const msg,
                                size_t sz)
{
    yella_agent* ag = (yella_agent*)agent;
    yella_message_part parts[2];
    size_t hdr_sz;

    mhdr->time = time(NULL);
    mhdr->sender = yella_text_dup(ag->state->id->text);
    mhdr->cmp = YELLA_COMPRESSION_LZ4;
    mhdr->seq.major = ag->state->boot_count;
    parts[0].data = yella_pack_mhdr(mhdr, &hdr_sz);
    parts[0].size = hdr_sz;
    parts[1].data = yella_lz4_compress(msg, &sz);
    parts[1].size = sz;
    yella_spool_push(ag->spool, parts, 2);
    free(parts[0].data);
    free(parts[1].data);
}

static void load_plugins(yella_agent* agent)
{
    yella_agent_api agent_api;
    yella_directory_iterator* itor;
    const char* cur;
    void* so;
    plugin_api* api;
    yella_plugin_start_func start;
    yella_plugin* plugin;

    agent_api.send_message = send_plugin_message;
    itor = yella_create_directory_iterator(yella_settings_get_text("agent", "api-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        so = open_shared_object(cur);
        if (so != NULL)
        {
            start = shared_object_symbol(so, "plugin_start");
            if (start != NULL)
            {
                api = malloc(sizeof(plugin_api));
                plugin = start(&agent_api, agent);
                if (plugin != NULL)
                {
                    api->name = yella_text_dup(plugin->name);
                    api->shared_object = so;
                    api->start_func = start;
                    api->status_func = shared_object_symbol(so, "plugin_status");
                    api->stop_func = shared_object_symbol(so, "plugin_stop");
                    if (api->status_func && api->stop_func)
                    {
                        yella_push_back_ptr_vector(agent->plugins, api);
                        CHUCHO_C_INFO("yella.agent",
                                      "Loaded plugin %s, version %s",
                                      plugin->name,
                                      plugin->version);
                    }
                    else
                    {
                        free(api);
                        close_shared_object(so);
                        CHUCHO_C_WARN("yella.agent",
                                      "plugin_status or plugin_stop is not defined in %s",
                                      cur);

                    }
                    yella_destroy_plugin(plugin);
                }
                else
                {
                    free(api);
                    close_shared_object(so);
                }
            }
            else
            {
                close_shared_object(so);
                CHUCHO_C_WARN("yella.agent",
                              "plugin_start is not defined in %s",
                              cur);
            }
        }
        else
        {
            CHUCHO_C_WARN("yella.agent",
                          "The file %s could not be loaded as a shared object",
                          cur);
        }
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
}

static void plugin_api_dtor(void* p, void* udata)
{
    plugin_api* w = (plugin_api*)p;

    CHUCHO_C_INFO("yella.agent", "Closing plugin %s", w->name);
    w->stop_func();
    free(w->name);
    close_shared_object(w->shared_object);
    free(w);
}

static void retrieve_agent_settings(void)
{
    yella_setting_desc descs[] =
    {
        { "data-dir", YELLA_SETTING_VALUE_TEXT },
        { "log-dir", YELLA_SETTING_VALUE_TEXT },
        { "plugin-dir", YELLA_SETTING_VALUE_TEXT },
        { "spool-dir", YELLA_SETTING_VALUE_TEXT },
        { "router", YELLA_SETTING_VALUE_TEXT },
        { "max-spool-partitions", YELLA_SETTING_VALUE_UINT },
        { "max-spool-partition-size", YELLA_SETTING_VALUE_UINT },
        { "reconnect-timeout-seconds", YELLA_SETTING_VALUE_UINT },
        { "poll-milliseconds", YELLA_SETTING_VALUE_UINT },
        { "heartbeat-seconds", YELLA_SETTING_VALUE_UINT }
    };

    yella_settings_set_uint("agent", "max-spool-partitions", 1000);
    yella_settings_set_uint("agent", "max-spool-partition-size", 2 * YELLA_MEGABYTE);
    yella_settings_set_uint("agent", "reconnect-timeout-seconds", 5);
    yella_settings_set_uint("agent", "poll-milliseconds", 500);
    yella_settings_set_uint("agent", "heartbeat-seconds", 30);
    yella_retrieve_settings("agent", descs, YELLA_ARRAY_SIZE(descs));
}

yella_agent* yella_create_agent(void)
{
    yella_agent* result;

    result = calloc(1, sizeof(yella_agent));
    result->should_stop = false;
    result->state = yella_load_saved_state();
    result->spool = yella_create_spool();
    if (result->spool == NULL)
    {
        CHUCHO_C_ERROR("yella.agent", "Unable to create spool");
        yella_destroy_saved_state(result->state);
        free(result);
        return NULL;
    }
    result->router = yella_create_router(result->state->id);
    result->plugins = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(result->plugins, plugin_api_dtor, NULL);
    yella_load_settings_doc();
    retrieve_agent_settings();
    load_plugins(result);
    yella_destroy_settings_doc();
    result->heartbeat = yella_create_thread(heartbeat_thr, result);
    return result;
}

void yella_destroy_agent(yella_agent* agent)
{
    agent->should_stop = true;
    yella_join_thread(agent->heartbeat);
    yella_destroy_thread(agent->heartbeat);
    yella_destroy_ptr_vector(agent->plugins);
    yella_destroy_saved_state(agent->state);
    yella_destroy_router(agent->router);
    yella_destroy_spool(agent->spool);
    free(agent);
}
