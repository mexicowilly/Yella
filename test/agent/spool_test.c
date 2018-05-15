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

#if defined(YELLA_POSIX)
#include "agent/platform/posix/yella_uuid_posix.c"
#endif
#include "agent/router.c"
#include "agent/saved_state.c"
#include "agent/spool.c"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

typedef struct test_state
{
    yella_router* rtr;
    yella_spool* sp;
    yella_saved_state* ss;
} test_state;

static void state_changed(yella_router_state st, void* data)
{
    if (st == YELLA_ROUTER_CONNECTED)
        yella_signal_event((yella_event*)data);
}

static int set_up(void** arg)
{
    test_state* targ;
    yella_event* state_event;

    *arg = malloc(sizeof(test_state));
    targ = *arg;
    yella_settings_set_text("router", "tcp://127.0.0.1:19567");
    yella_settings_set_uint("reconnect-timeout-seconds", 5);
    yella_settings_set_uint("poll-milliseconds", 500);
    targ->ss = yella_load_saved_state();
    targ->rtr = yella_create_router(yella_saved_state_uuid(targ->ss));
    state_event = yella_create_event();
    yella_set_router_state_callback(targ->rtr, state_changed, state_event);
    yella_wait_for_event(state_event);
    yella_destroy_event(state_event);
    targ->sp = yella_create_spool(targ->ss);
    return 0;
}

static int tear_down(void** arg)
{
    test_state* targ;

    targ = *arg;
    yella_destroy_spool(targ->sp);
    yella_destroy_router(targ->rtr);
    yella_destroy_saved_state(targ->ss);
    free(targ);
    return 0;
}

int main()
{

}