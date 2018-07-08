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
#include "common/settings.h"
#include "common/message_header.h"
#include "common/text_util.h"
#include "common/compression.h"
#include "common/message_part.h"
#include <lz4.h>
#include <stdlib.h>

static const uint64_t YELLA_MEGABYTE = 1024 * 1024;
static const uint64_t YELLA_GIGABYTE = 1024 * 1024 * 1024;

typedef struct yella_agent
{
    yella_saved_state* state;
    yella_router* router;
    yella_spool* spool;
} yella_agent;

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
        { "poll-milliseconds", YELLA_SETTING_VALUE_UINT }
    };

    yella_settings_set_uint("agent", "max-spool-partitions", 1000);
    yella_settings_set_uint("agent", "max-spool-partition-size", 2 * YELLA_MEGABYTE);
    yella_settings_set_uint("agent", "reconnect-timeout-seconds", 5);
    yella_settings_set_uint("agent", "poll-milliseconds", 500);
    yella_retrieve_settings("agent", descs, sizeof(descs) / sizeof(descs[0]));
}

yella_agent* yella_create_agent(void)
{
    yella_agent* result;

    result = calloc(1, sizeof(yella_agent));
    result->state = yella_load_saved_state();
    yella_load_settings_doc();
    retrieve_agent_settings();

    /* load plugins */

    yella_destroy_settings_doc();
    return result;
}

void yella_destroy_agent(yella_agent* agent)
{
    yella_destroy_saved_state(agent->state);
    free(agent);
}
