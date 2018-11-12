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
#include "agent/kafka.h"
#include "agent/yella_uuid.h"
#include "agent/saved_state.h"
#include "agent/runtime_link.h"
#include "agent/heartbeat.h"
#include "agent/envelope.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/thread.h"
#include "common/macro_util.h"
#include "common/text_util.h"
#include "plugin/plugin.h"
#include "common/sglib.h"
#include "spool.h"
#include <stdlib.h>
#include <chucho/log.h>
#include <stdatomic.h>
#include <time.h>

static const uint64_t YELLA_MEGABYTE = 1024 * 1024;
static const uint64_t YELLA_GIGABYTE = 1024 * 1024 * 1024;

typedef struct in_handler
{
    uds key;
    yella_in_cap_handler func;
    char color;
    struct in_handler* left;
    struct in_handler* right;
} in_handler;

typedef struct plugin_api
{
    uds name;
    void* shared_object;
    yella_plugin_start_func start_func;
    yella_plugin_status_func status_func;
    yella_plugin_stop_func stop_func;
} plugin_api;

typedef struct yella_agent
{
    yella_saved_state* state;
    yella_ptr_vector* plugins;
    yella_thread* heartbeat;
    atomic_bool should_stop;
    in_handler* in_handlers;
    chucho_logger_t* lgr;
    kafka* kf;
} yella_agent;

#define YELLA_KEY_COMPARE(x, y) u_strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(in_handler, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(in_handler, left, right, color, YELLA_KEY_COMPARE);

static void plugin_dtor(void* plg, void* udata)
{
    yella_destroy_plugin((yella_plugin*)plg);
}

static void heartbeat_thr(void* udata)
{
    time_t next;
    size_t to_wait = *yella_settings_get_uint(u"agent", u"heartbeat-seconds");
    yella_agent* ag = (yella_agent*)udata;
    plugin_api* api;
    yella_plugin* plg;
    int i;
    yella_ptr_vector* plugins;
    yella_envelope* env;
    message_part part;
    message_part env_part;
    const UChar* heartbeat_topic = yella_settings_get_text(u"agent", u"heartbeat-topic");

    next = 0;
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
        plugins = yella_create_ptr_vector();
        yella_set_ptr_vector_destructor(plugins, plugin_dtor, NULL);
        for (i = 0; i < yella_ptr_vector_size(ag->plugins); i++)
        {
            api = (plugin_api*)yella_ptr_vector_at(ag->plugins, i);
            yella_push_back_ptr_vector(plugins, api->status_func());
        }
        part.data = create_heartbeat(ag->state->id->text, plugins, &part.size);
        env = yella_create_envelope(ag->state->id->text, heartbeat_topic, part.data, part.size);
        env_part.data = yella_pack_envelope(env, &env_part.size);
        yella_destroy_envelope(env);
        yella_destroy_ptr_vector(plugins);
        if (send_transient_kafka_message(ag->kf, u"yella.heartbeat", env_part.data, env_part.size))
            CHUCHO_C_INFO_L(ag->lgr, "Sent heartbeat");
        else
            CHUCHO_C_INFO_L(ag->lgr, "Error sending heartbeat");
        free(env_part.data);
        next = time(NULL) + to_wait;
    } while (true);
}

static void send_plugin_message(void* agent,
                                const UChar* const tpc,
                                const uint8_t* const msg,
                                size_t sz)
{
    yella_agent* ag;

    ag = (yella_agent*)agent;
    send_kafka_message(ag->kf, tpc, (void*)msg, sz);
}

static void load_plugins(yella_agent* agent)
{
    yella_agent_api agent_api;
    yella_directory_iterator* itor;
    const UChar* cur;
    void* so;
    plugin_api* api;
    yella_plugin_start_func start;
    yella_plugin* plugin;
    in_handler hndlr_to_find;
    in_handler* hndlr_found;
    int i;
    yella_plugin_in_cap* in_cap;
    char* utf8_n;
    char* utf8_v;

    agent_api.send_message = send_plugin_message;
    itor = yella_create_directory_iterator(yella_settings_get_text(u"agent", u"plugin-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        so = open_shared_object(cur, agent->lgr);
        if (so != NULL)
        {
            start = shared_object_symbol(so, u"plugin_start", agent->lgr);
            if (start != NULL)
            {
                api = malloc(sizeof(plugin_api));
                plugin = start(&agent_api, agent);
                if (plugin != NULL)
                {
                    api->name = udsnew(plugin->name);
                    api->shared_object = so;
                    api->start_func = start;
                    api->status_func = shared_object_symbol(so, u"plugin_status", agent->lgr);
                    api->stop_func = shared_object_symbol(so, u"plugin_stop", agent->lgr);
                    if (api->status_func && api->stop_func)
                    {
                        yella_push_back_ptr_vector(agent->plugins, api);
                        for (i = 0; i < yella_ptr_vector_size(plugin->in_caps); i++)
                        {
                            in_cap = (yella_plugin_in_cap*)yella_ptr_vector_at(plugin->in_caps, i);
                            hndlr_to_find.key = in_cap->name;
                            hndlr_found = sglib_in_handler_find_member(agent->in_handlers, &hndlr_to_find);
                            if (hndlr_found == NULL)
                            {
                                hndlr_found = malloc(sizeof(in_handler));
                                hndlr_found->key = udsnew(in_cap->name);
                                sglib_in_handler_add(&agent->in_handlers, hndlr_found);
                            }
                            hndlr_found->func = in_cap->handler;
                        }
                        utf8_n = yella_to_utf8(plugin->name);
                        utf8_v = yella_to_utf8(plugin->version);
                        CHUCHO_C_INFO_L(agent->lgr,
                                        "Loaded plugin %s, version %s",
                                        utf8_n,
                                        utf8_v);
                        free(utf8_n);
                        free(utf8_v);
                    }
                    else
                    {
                        free(api);
                        close_shared_object(so);
                        utf8_n = yella_to_utf8(cur);
                        CHUCHO_C_WARN_L(agent->lgr,
                                        "plugin_status or plugin_stop is not defined in %s",
                                        utf8_n);
                        free(utf8_n);
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
                utf8_n = yella_to_utf8(cur);
                CHUCHO_C_WARN_L(agent->lgr,
                                "plugin_start is not defined in %s",
                                utf8_n);
                free(utf8_n);
            }
        }
        else
        {
            utf8_n = yella_to_utf8(cur);
            CHUCHO_C_WARN_L(agent->lgr,
                            "The file %s could not be loaded as a shared object",
                            utf8_n);
            free(utf8_n);
        }
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
}

static void message_received(void* msg, size_t len, void* udata)
{
    yella_agent* ag;
    in_handler to_find;
    in_handler* hndlr;
    yella_envelope* env;
    char* utf8;
    yella_rc rc;

    ag = (yella_agent*)udata;
    env = yella_unpack_envelope(msg);
    to_find.key = env->type;
    hndlr = sglib_in_handler_find_member(ag->in_handlers, &to_find);
    if (hndlr == NULL)
    {
        utf8 = yella_to_utf8(env->type);
        CHUCHO_C_ERROR_L(ag->lgr, "This message type is not registered: %s", utf8);
        free(utf8);
    }
    else
    {
        rc = hndlr->func(msg, len);
        if (rc != YELLA_NO_ERROR)
        {
            utf8 = yella_to_utf8(env->type);
            CHUCHO_C_ERROR_L(ag->lgr, "Error handling message %s: %s", utf8, yella_strerror(rc));
            free(utf8);
        }
    }
}

static void plugin_api_dtor(void* p, void* udata)
{
    plugin_api* w = (plugin_api*)p;
    char* utf8;

    utf8 = yella_to_utf8(w->name);
    CHUCHO_C_INFO("yella.agent", "Closing plugin %s", utf8);
    free(utf8);
    w->stop_func();
    udsfree(w->name);
    close_shared_object(w->shared_object);
    free(w);
}

static void retrieve_agent_settings(void)
{
    yella_setting_desc descs[] =
    {
        { u"data-dir", YELLA_SETTING_VALUE_TEXT },
        { u"plugin-dir", YELLA_SETTING_VALUE_TEXT },
        { u"spool-dir", YELLA_SETTING_VALUE_TEXT },
        { u"max-spool-partitions", YELLA_SETTING_VALUE_UINT },
        { u"max-spool-partition-size", YELLA_SETTING_VALUE_UINT },
        { u"reconnect-timeout-seconds", YELLA_SETTING_VALUE_UINT },
        { u"poll-milliseconds", YELLA_SETTING_VALUE_UINT },
        { u"heartbeat-seconds", YELLA_SETTING_VALUE_UINT },
        { u"brokers", YELLA_SETTING_VALUE_TEXT },
        { u"kafka-debug-contexts", YELLA_SETTING_VALUE_TEXT },
        { u"latency-milliseconds", YELLA_SETTING_VALUE_UINT },
        { u"compression-type", YELLA_SETTING_VALUE_TEXT },
        { u"connection-interval-seconds", YELLA_SETTING_VALUE_UINT },
        { u"agent-topic", YELLA_SETTING_VALUE_TEXT },
        { u"heartbeat-topic", YELLA_SETTING_VALUE_TEXT }
    };

    yella_settings_set_uint(u"agent", u"max-spool-partitions", 1000);
    yella_settings_set_uint(u"agent", u"max-spool-partition-size", 2 * YELLA_MEGABYTE);
    yella_settings_set_uint(u"agent", u"reconnect-timeout-seconds", 5);
    yella_settings_set_uint(u"agent", u"heartbeat-seconds", 30);
    yella_settings_set_text(u"agent", u"compression-type", u"lz4");
    yella_settings_set_uint(u"agent", u"connection-interval-seconds", 15);
    yella_settings_set_text(u"agent", u"agent-topic", u"yella.agent");
    yella_settings_set_text(u"agent", u"heartbeat-topic", u"yella.heartbeat");
    yella_retrieve_settings(u"agent", descs, YELLA_ARRAY_SIZE(descs));
}

yella_agent* yella_create_agent(void)
{
    yella_agent* result;
    const UChar* dirs[2];
    int i;
    yella_rc yrc;
    char* utf8;

    yella_load_settings_doc();
    retrieve_agent_settings();
    dirs[0] = yella_settings_get_text(u"agent", u"data-dir");
    dirs[1] = yella_settings_get_text(u"agent", u"plugin-dir");
    for (i = 0; i < 2; i++)
    {
        yrc = yella_ensure_dir_exists(dirs[i]);
        if (yrc != YELLA_NO_ERROR)
        {
            utf8 = yella_to_utf8(dirs[i]);
            CHUCHO_C_ERROR("yella.agent", "Could not create %s: %s", utf8, yella_strerror(yrc));
            free(utf8);
            return NULL;
        }
    }
    result = calloc(1, sizeof(yella_agent));
    result->should_stop = false;
    result->lgr = chucho_get_logger("yella.agent");
    result->state = yella_load_saved_state(result->lgr);
    yella_save_saved_state(result->state, result->lgr);
    if (yella_settings_get_text(u"agent", u"router") == NULL)
    {
        chucho_release_logger(result->lgr);
        yella_destroy_saved_state(result->state);
        free(result);
        return NULL;
    }
    result->kf = create_kafka(result->state->id);
    result->plugins = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(result->plugins, plugin_api_dtor, NULL);
    load_plugins(result);
    yella_destroy_settings_doc();
    result->heartbeat = yella_create_thread(heartbeat_thr, result);
    set_kafka_message_handler(result->kf, message_received, result);
    return result;
}

void yella_destroy_agent(yella_agent* agent)
{
    in_handler* hndlr;
    struct sglib_in_handler_iterator itor;

    agent->should_stop = true;
    yella_join_thread(agent->heartbeat);
    yella_destroy_thread(agent->heartbeat);
    for (hndlr = sglib_in_handler_it_init(&itor, agent->in_handlers);
         hndlr != NULL;
         hndlr = sglib_in_handler_it_next(&itor))
    {
        udsfree(hndlr->key);
        free(hndlr);
    }
    yella_destroy_ptr_vector(agent->plugins);
    yella_destroy_saved_state(agent->state);
    destroy_kafka(agent->kf);
    chucho_release_logger(agent->lgr);
    free(agent);
}
