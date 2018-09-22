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
#include "agent/heartbeat.h"
#include "common/settings.h"
#include "common/message_header.h"
#include "common/text_util.h"
#include "common/compression.h"
#include "common/message_part.h"
#include "common/file.h"
#include "common/thread.h"
#include "common/macro_util.h"
#include "plugin/plugin.h"
#include "common/sglib.h"
#include <lz4.h>
#include <stdlib.h>
#include <chucho/log.h>
#include <stdatomic.h>

static const uint64_t YELLA_MEGABYTE = 1024 * 1024;
static const uint64_t YELLA_GIGABYTE = 1024 * 1024 * 1024;

typedef struct in_handler
{
    char* key;
    yella_in_cap_handler func;
    char color;
    struct in_handler* left;
    struct in_handler* right;
} in_handler;

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
    yella_thread* spool_consumer;
    in_handler* in_handlers;
    chucho_logger_t* lgr;
} yella_agent;

#define YELLA_KEY_COMPARE(x, y) strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(in_handler, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(in_handler, left, right, color, YELLA_KEY_COMPARE);

static void plugin_dtor(void* plg, void* udata)
{
    yella_destroy_plugin((yella_plugin*)plg);
}

static void heartbeat_thr(void* udata)
{
    time_t next;
    size_t to_wait = *yella_settings_get_uint("agent", "hearbeat-seconds");
    yella_agent* ag = (yella_agent*)udata;
    plugin_api* api;
    yella_plugin* plg;
    int i;
    yella_ptr_vector* plugins;
    yella_sender* sndr;
    yella_message_part parts[2];
    yella_message_header* mhdr;
    uint32_t minor_seq;

    minor_seq = 0;
    sndr = yella_create_sender(ag->router);
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
        if (yella_router_get_state(ag->router) == YELLA_ROUTER_CONNECTED)
        {
            plugins = yella_create_ptr_vector();
            yella_set_ptr_vector_destructor(plugins, plugin_dtor, NULL);
            for (i = 0; i < yella_ptr_vector_size(ag->plugins); i++)
            {
                api = (plugin_api*)yella_ptr_vector_at(ag->plugins, i);
                yella_push_back_ptr_vector(plugins, api->status_func());
            }
            parts[1].data = create_heartbeat(ag->state->id->text, plugins, &parts[1].size);
            yella_destroy_ptr_vector(plugins);
            mhdr = yella_create_mhdr();
            mhdr->time = time(NULL);
            mhdr->sender = yella_text_dup(ag->state->id->text);
            mhdr->type = yella_text_dup("yella.heartbeat");
            mhdr->cmp = YELLA_COMPRESSION_NONE;
            mhdr->seq.major = ag->state->boot_count;
            mhdr->seq.minor = ++minor_seq;
            parts[0].data = yella_pack_mhdr(mhdr, &parts[0].size);
            if (yella_send(sndr, parts, 2))
                CHUCHO_C_INFO_L(ag->lgr, "Sent heartbeat");
            else
                CHUCHO_C_INFO_L(ag->lgr, "Error sending heartbeat");
        }
        else
        {
            CHUCHO_C_INFO_L(ag->lgr,
                            "Not sending heartbeat because there is no router connection");
        }
        next = time(NULL) + to_wait;
    } while (true);
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
    in_handler hndlr_to_find;
    in_handler* hndlr_found;
    int i;
    yella_plugin_in_cap* in_cap;

    agent_api.send_message = send_plugin_message;
    itor = yella_create_directory_iterator(yella_settings_get_text("agent", "plugin-dir"));
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        so = open_shared_object(cur, agent->lgr);
        if (so != NULL)
        {
            start = shared_object_symbol(so, "plugin_start", agent->lgr);
            if (start != NULL)
            {
                api = malloc(sizeof(plugin_api));
                plugin = start(&agent_api, agent);
                if (plugin != NULL)
                {
                    api->name = yella_text_dup(plugin->name);
                    api->shared_object = so;
                    api->start_func = start;
                    api->status_func = shared_object_symbol(so, "plugin_status", agent->lgr);
                    api->stop_func = shared_object_symbol(so, "plugin_stop", agent->lgr);
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
                                hndlr_found->key = yella_text_dup(in_cap->name);
                                sglib_in_handler_add(&agent->in_handlers, hndlr_found);
                            }
                            hndlr_found->func = in_cap->handler;
                        }
                        CHUCHO_C_INFO_L(agent->lgr,
                                        "Loaded plugin %s, version %s",
                                        plugin->name,
                                        plugin->version);
                    }
                    else
                    {
                        free(api);
                        close_shared_object(so);
                        CHUCHO_C_WARN_L(agent->lgr,
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
                CHUCHO_C_WARN_L(agent->lgr,
                                "plugin_start is not defined in %s",
                                cur);
            }
        }
        else
        {
            CHUCHO_C_WARN_L(agent->lgr,
                            "The file %s could not be loaded as a shared object",
                            cur);
        }
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
}

static void message_received(const yella_message_part* const hdr, const yella_message_part* const body, void* udata)
{
    in_handler to_find;
    in_handler* found;
    yella_message_header* mhdr;
    yella_agent* ag;
    uint8_t* decmp;
    size_t decmp_sz;
    in_handler* hndlr;
    in_handler hndlr_to_find;
    yella_rc rc;

    mhdr = yella_unpack_mhdr(hdr->data);
    ag = (yella_agent*)udata;
    yella_log_mhdr(mhdr, ag->lgr);
    hndlr_to_find.key = mhdr->type;
    hndlr = sglib_in_handler_find_member(ag->in_handlers, &hndlr_to_find);
    if (hndlr == NULL)
    {
        CHUCHO_C_ERROR_L(ag->lgr, "This message type is not registered: %s", mhdr->type);
    }
    else
    {
        if (mhdr->cmp == YELLA_COMPRESSION_LZ4)
        {
            decmp_sz = body->size;
            decmp = yella_lz4_decompress(body->data, &decmp_sz);
            rc = hndlr->func(mhdr, decmp, decmp_sz);
            free(decmp);
        }
        else
        {
            rc = hndlr->func(mhdr, body->data, body->size);
        }
        if (rc != YELLA_NO_ERROR)
            CHUCHO_C_ERROR_L(ag->lgr, "Error handling message %s: %s", mhdr->type, yella_strerror(rc));
    }
    yella_destroy_mhdr(mhdr);
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

static void spool_thr(void* udata)
{
    yella_agent* ag;
    yella_message_part* popped;
    size_t num_popped;
    yella_rc rc;
    yella_sender* sndr;
    int i;

    ag = (yella_agent*)udata;
    sndr = yella_create_sender(ag->router);
    while (!ag->should_stop)
    {
        while (yella_router_get_state(ag->router) != YELLA_ROUTER_CONNECTED)
        {
            yella_sleep_this_thread(250);
            if (ag->should_stop)
                goto get_out;
        }
        rc = yella_spool_pop(ag->spool,
                             500,
                             &popped,
                             &num_popped);
        if (rc == YELLA_NO_ERROR)
        {
            yella_send(sndr, popped, num_popped);
            for (i = 0; i < num_popped; i++)
                free(popped[i].data);
            free(popped);
        }
        else if (rc != YELLA_TIMED_OUT)
        {
            // TODO: error
        }
    }
get_out:
    yella_destroy_sender(sndr);
}

yella_agent* yella_create_agent(void)
{
    yella_agent* result;

    yella_load_settings_doc();
    retrieve_agent_settings();
    result = calloc(1, sizeof(yella_agent));
    result->should_stop = false;
    result->lgr = chucho_get_logger("yella.agent");
    result->state = yella_load_saved_state(result->lgr);
    yella_save_saved_state(result->state, result->lgr);
    result->spool = yella_create_spool();
    if (result->spool == NULL)
    {
        CHUCHO_C_ERROR_L(result->lgr, "Unable to create spool");
        chucho_release_logger(result->lgr);
        yella_destroy_saved_state(result->state);
        free(result);
        return NULL;
    }
    if (yella_settings_get_text("agent", "router") == NULL)
    {
        chucho_release_logger(result->lgr);
        yella_destroy_saved_state(result->state);
        free(result);
        return NULL;
    }
    result->router = yella_create_router(result->state->id);
    result->plugins = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(result->plugins, plugin_api_dtor, NULL);
    load_plugins(result);
    yella_destroy_settings_doc();
    result->spool_consumer = yella_create_thread(spool_thr, result);
    result->heartbeat = yella_create_thread(heartbeat_thr, result);
    yella_set_router_message_received_callback(result->router, message_received, result);
    return result;
}

void yella_destroy_agent(yella_agent* agent)
{
    in_handler* hndlr;
    struct sglib_in_handler_iterator itor;

    agent->should_stop = true;
    yella_join_thread(agent->heartbeat);
    yella_destroy_thread(agent->heartbeat);
    yella_join_thread(agent->spool_consumer);
    yella_destroy_thread(agent->spool_consumer);
    for (hndlr = sglib_in_handler_it_init(&itor, agent->in_handlers);
         hndlr != NULL;
         hndlr = sglib_in_handler_it_next(&itor))
    {
        free(hndlr->key);
        free(hndlr);
    }
    yella_destroy_ptr_vector(agent->plugins);
    yella_destroy_saved_state(agent->state);
    yella_destroy_router(agent->router);
    yella_destroy_spool(agent->spool);
    chucho_release_logger(agent->lgr);
    free(agent);
}
