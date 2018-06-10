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
#include "common/settings.h"
#include <stdlib.h>

static const uint64_t YELLA_MEGABYTE = 1024 * 1024;
static const uint64_t YELLA_GIGABYTE = 1024 * 1024 * 1024;

struct yella_agent
{
    yella_uuid* identity;
    yella_router* router;
};

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

    yella_settings_set_uint("max-spool-partitions", 1000);
    yella_settings_set_uint("max-spool-partition-size", 2 * YELLA_MEGABYTE);
    yella_settings_set_uint("reconnect-timeout-seconds", 5);
    yella_settings_set_uint("poll-milliseconds", 500);
    yella_retrieve_settings(descs, sizeof(descs) / sizeof(descs[0]));
}

yella_agent* yella_create_agent(void)
{
    yella_agent* result;
    yella_saved_state* state;

    result = malloc(sizeof(yella_agent));
    memset(result, 0, sizeof(yella_agent));
    yella_load_settings_doc();
    retrieve_agent_settings();
    state = yella_load_saved_state();
    yella_destroy_saved_state(state);

    /* load plugins */

    yella_destroy_settings_doc();
    return result;
}

void yella_destroy_agent(yella_agent* agent)
{
    free(agent);
}
