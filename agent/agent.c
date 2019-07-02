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
#include "agent/runtime_link.h"
#include "agent/heartbeat.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/thread.h"
#include "common/macro_util.h"
#include "common/text_util.h"
#include "common/compression.h"
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
    void* udata;
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
    void* udata;
} plugin_api;

typedef struct plugin_sender
{
    void* thread;
    sender* sndr;
    char color;
    struct plugin_sender* left;
    struct plugin_sender* right;
} plugin_sender;

typedef struct yella_agent
{
    yella_saved_state* state;
    yella_ptr_vector* plugins;
    yella_thread* heartbeat;
    atomic_bool should_stop;
    in_handler* in_handlers;
    chucho_logger_t* lgr;
    router* rtr;
    plugin_sender* plugin_senders;
} yella_agent;

#define YELLA_KEY_COMPARE(x, y) u_strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(in_handler, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(in_handler, left, right, color, YELLA_KEY_COMPARE);

#define YELLA_SENDER_COMPARE(x, y) (x->thread - y->thread)

SGLIB_DEFINE_RBTREE_PROTOTYPES(plugin_sender, left, right, color, YELLA_SENDER_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(plugin_sender, left, right, color, YELLA_SENDER_COMPARE);

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
    int i;
    yella_ptr_vector* plugins;
    sender* sndr;
    yella_message_part parts[2];
    yella_message_header* mhdr;
    uint32_t minor_seq;

    CHUCHO_C_INFO(ag->lgr, "The hearbeat thread is starting");
    minor_seq = 0;
    sndr = create_sender(ag->rtr);
    next = 0;
    do
    {
        while (time(NULL) < next)
        {
            yella_sleep_this_thread_milliseconds(1000);
            if (ag->should_stop)
                break;
        }
        if (ag->should_stop)
            break;
        if (get_router_state(ag->rtr) == ROUTER_CONNECTED)
        {
            plugins = yella_create_ptr_vector();
            yella_set_ptr_vector_destructor(plugins, plugin_dtor, NULL);
            for (i = 0; i < yella_ptr_vector_size(ag->plugins); i++)
            {
                api = (plugin_api*)yella_ptr_vector_at(ag->plugins, i);
                yella_push_back_ptr_vector(plugins, api->status_func(ag));
            }
            parts[1].data = create_heartbeat(ag->state->id->text, plugins, &parts[1].size);
            yella_destroy_ptr_vector(plugins);
            mhdr = yella_create_mhdr();
            mhdr->time = time(NULL);
            mhdr->sender = udsnew(ag->state->id->text);
            mhdr->recipient = udsnew(yella_settings_get_text(u"agent", u"heartbeat-recipient"));
            mhdr->type = udsnew(u"yella.heartbeat");
            mhdr->cmp = YELLA_COMPRESSION_NONE;
            mhdr->seq.major = ag->state->boot_count;
            mhdr->seq.minor = ++minor_seq;
            parts[0].data = yella_pack_mhdr(mhdr, &parts[0].size);
            yella_destroy_mhdr(mhdr);
            if (send_transient_router_message(sndr, parts, 2))
                CHUCHO_C_INFO(ag->lgr, "Sent heartbeat");
            else
                CHUCHO_C_INFO(ag->lgr, "Error sending heartbeat");
        }
        else
        {
            CHUCHO_C_INFO(ag->lgr,
                          "Not sending heartbeat because there is no router connection");
        }
        next = time(NULL) + to_wait;
    } while (true);
    destroy_sender(sndr);
    CHUCHO_C_INFO(ag->lgr, "The hearbeat thread is ending");
}

static void send_plugin_message(void* agent,
                                yella_message_header* mhdr,
                                uint8_t* msg,
                                size_t sz)
{
    yella_agent* ag;
    yella_message_part parts[2];
    size_t hsz;
    plugin_sender to_find;
    plugin_sender* found;

    ag = (yella_agent*)agent;
    mhdr->sender = udsnew(ag->state->id->text);
    mhdr->seq.major = ag->state->boot_count;
    to_find.thread = yella_this_thread();
    found = sglib_plugin_sender_find_member(ag->plugin_senders, &to_find);
    if (found == NULL)
    {
        found = malloc(sizeof(plugin_sender));
        found->thread = to_find.thread;
        found->sndr = create_sender(ag->rtr);
        sglib_plugin_sender_add(&ag->plugin_senders, found);
    }
    parts[0].data = yella_pack_mhdr(mhdr, &hsz);
    parts[0].size = hsz;
    if (mhdr->cmp == YELLA_COMPRESSION_LZ4)
    {
        parts[1].size = sz;
        parts[1].data = yella_lz4_compress(msg, &parts[1].size);
    }
    else
    {
        parts[1].data = (uint8_t*)msg;
        parts[1].size = sz;
    }
    send_router_message(found->sndr, parts, 2);
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
    itor = yella_create_directory_iterator(yella_settings_get_dir(u"agent", u"plugin-dir"));
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
                    api->udata = plugin->udata;
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
                            hndlr_found->udata = in_cap->udata;
                        }
                        utf8_n = yella_to_utf8(plugin->name);
                        utf8_v = yella_to_utf8(plugin->version);
                        CHUCHO_C_INFO_L(agent->lgr,
                                        "Loaded plugin '%s', version '%s'",
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
    yella_log_settings();
}

static void message_received(const yella_message_part* const hdr, const yella_message_part* const body, void* udata)
{
    in_handler to_find;
    in_handler* found;
    yella_message_header* mhdr;
    yella_agent* ag;
    in_handler* hndlr;
    in_handler hndlr_to_find;
    yella_rc rc;
    char* utf8;
    yella_message_part part;

    mhdr = yella_unpack_mhdr(hdr->data);
    ag = (yella_agent*)udata;
    yella_log_mhdr(mhdr, ag->lgr);
    hndlr_to_find.key = mhdr->type;
    hndlr = sglib_in_handler_find_member(ag->in_handlers, &hndlr_to_find);
    if (hndlr == NULL)
    {
        utf8 = yella_to_utf8(mhdr->type);
        CHUCHO_C_ERROR_L(ag->lgr, "This message type is not registered: %s", utf8);
        free(utf8);
    }
    else
    {
        if (mhdr->cmp == YELLA_COMPRESSION_LZ4)
        {
            part.size = body->size;
            part.data = yella_lz4_decompress(body->data, &part.size);
            rc = hndlr->func(mhdr, &part, hndlr->udata);
            free(part.data);
        }
        else
        {
            rc = hndlr->func(mhdr, body, hndlr->udata);
        }
        if (rc != YELLA_NO_ERROR)
        {
            utf8 = yella_to_utf8(mhdr->type);
            CHUCHO_C_ERROR_L(ag->lgr, "Error handling message '%s': %s", utf8, yella_strerror(rc));
            free(utf8);
        }
    }
    yella_destroy_mhdr(mhdr);
}

static void plugin_api_dtor(void* p, void* udata)
{
    plugin_api* w = (plugin_api*)p;
    char* utf8;

    utf8 = yella_to_utf8(w->name);
    CHUCHO_C_INFO("yella.agent", "Closing plugin %s", utf8);
    free(utf8);
    w->stop_func(w->udata);
    udsfree(w->name);
    close_shared_object(w->shared_object);
    free(w);
}

static void retrieve_agent_settings(void)
{
    yella_setting_desc descs[] =
    {
        { u"data-dir", YELLA_SETTING_VALUE_DIR },
        { u"plugin-dir", YELLA_SETTING_VALUE_DIR },
        { u"spool-dir", YELLA_SETTING_VALUE_DIR },
        { u"bin-dir", YELLA_SETTING_VALUE_DIR },
        { u"max-spool-partitions", YELLA_SETTING_VALUE_UINT },
        { u"max-spool-partition-size", YELLA_SETTING_VALUE_UINT },
        { u"heartbeat-seconds", YELLA_SETTING_VALUE_UINT },
        { u"router", YELLA_SETTING_VALUE_TEXT },
        { u"start-connection-seconds", YELLA_SETTING_VALUE_UINT },
        { u"max-message-size", YELLA_SETTING_VALUE_UINT },
        { u"reconnect-timeout-seconds", YELLA_SETTING_VALUE_UINT },
        { u"poll-milliseconds", YELLA_SETTING_VALUE_UINT },
        { u"heartbeat-recipient", YELLA_SETTING_VALUE_TEXT }
    };

    yella_settings_set_uint(u"agent", u"max-spool-partitions", 1000);
    yella_settings_set_uint(u"agent", u"max-spool-partition-size", 2 * YELLA_MEGABYTE);
    yella_settings_set_uint(u"agent", u"heartbeat-seconds", 30);
    yella_settings_set_uint(u"agent", u"start-connection-seconds", 2);
    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1M");
    yella_settings_set_uint(u"agent", u"reconnect-timeout-seconds", 5);
    yella_settings_set_uint(u"agent", u"poll-milliseconds", 500);
    yella_settings_set_text(u"agent", u"heartbeat-recipient", u"yella.stethoscope");

    yella_retrieve_settings(u"agent", descs, YELLA_ARRAY_SIZE(descs));
}

static void maybe_wait_for_router(yella_agent* ag)
{
    time_t stop;

    stop = time(NULL) + *yella_settings_get_uint(u"agent", u"start-connection-seconds");
    while (get_router_state(ag->rtr) != ROUTER_CONNECTED && time(NULL) < stop)
    {
        CHUCHO_C_INFO_L(ag->lgr, "Waiting for router connection");
        yella_sleep_this_thread_milliseconds(250);
    }
    if (get_router_state(ag->rtr) != ROUTER_CONNECTED)
        CHUCHO_C_INFO_L(ag->lgr, "Initial router connection failed");
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
    dirs[0] = yella_settings_get_dir(u"agent", u"data-dir");
    dirs[1] = yella_settings_get_dir(u"agent", u"plugin-dir");
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
    result->rtr = create_router(result->state->id);
    if (result->rtr == NULL)
    {
        chucho_release_logger(result->lgr);
        yella_destroy_saved_state(result->state);
        free(result);
        return NULL;
    }
    maybe_wait_for_router(result);
    result->plugins = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(result->plugins, plugin_api_dtor, NULL);
    load_plugins(result);
    yella_destroy_settings_doc();
    result->heartbeat = yella_create_thread(heartbeat_thr, result);
    set_router_message_received_callback(result->rtr, message_received, result);
    return result;
}

void yella_destroy_agent(yella_agent* agent)
{
    in_handler* hndlr;
    struct sglib_in_handler_iterator itor;
    plugin_sender* sndr;
    struct sglib_plugin_sender_iterator sitor;

    agent->should_stop = true;
    yella_join_thread(agent->heartbeat);
    yella_destroy_thread(agent->heartbeat);
    for (sndr = sglib_plugin_sender_it_init(&sitor, agent->plugin_senders);
         sndr != NULL;
         sndr = sglib_plugin_sender_it_next(&sitor))
    {
        destroy_sender(sndr->sndr);
        free(sndr);
    }
    destroy_router(agent->rtr);
    for (hndlr = sglib_in_handler_it_init(&itor, agent->in_handlers);
         hndlr != NULL;
         hndlr = sglib_in_handler_it_next(&itor))
    {
        udsfree(hndlr->key);
        free(hndlr);
    }
    yella_destroy_ptr_vector(agent->plugins);
    yella_destroy_saved_state(agent->state);
    chucho_release_logger(agent->lgr);
    free(agent);
}
