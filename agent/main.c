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

#include "common/settings.h"
#include "common/macro_util.h"
#include "common/thread.h"
#include "agent/signal_handler.h"
#include "agent/agent.h"
#define OPTPARSE_API static
#define OPTPARSE_IMPLEMENTATION
#include "agent/optparse.h"
#include <chucho/configuration.h>
#include <chucho/finalize.h>
#include <chucho/log.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void process_command_line(int argc, char* argv[])
{
    struct optparse_long opts[] =
    {
        { "config-file", 'c', OPTPARSE_OPTIONAL },
        { "help", 'h', OPTPARSE_NONE },
        { 0 }
    };
    struct optparse parser;
    int opt;

    optparse_init(&parser, argv);
    opt = optparse_long(&parser, opts, NULL);
    while (opt != -1)
    {
        switch (opt)
        {
        case 'c':
            yella_settings_set_text("agent", "config-file", parser.optarg);
            break;
        case '?':
            printf("Error: %s\n", parser.errmsg);
            /* fall through */
        case 'h':
            printf("Accepted options are:\n");
            printf("  -c,--config-file\tThe name of the config file\n");
            printf("  -h,--help\t\tPrint this helpful message\n");
            exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
        }
        opt = optparse_long(&parser, opts, NULL);
    }
}

static void term_handler(void* udata)
{
    yella_signal_event((yella_event*)udata);
}

int main(int argc, char* argv[])
{
    yella_event* term_evt;
    yella_agent* agent;
    int rc;

    install_signal_handler();
    yella_initialize_settings();
    process_command_line(argc, argv);
    chucho_cnf_set_file_name(yella_settings_get_text("agent", "config-file"));
    CHUCHO_C_INFO("yella.agent",
                  "Yella version " YELLA_VALUE_STR(YELLA_VERSION) " is starting");
    term_evt = yella_create_event();
    set_signal_termination_handler(term_handler, term_evt);
    agent = yella_create_agent();
    if (agent == NULL)
    {
        CHUCHO_C_ERROR("yella.agent", "Unable to create the agent");
        rc = EXIT_FAILURE;
    }
    else
    {
        yella_wait_for_event(term_evt);
        yella_destroy_agent(agent);
        rc = EXIT_SUCCESS;
    }
    yella_destroy_event(term_evt);
    yella_destroy_settings();
    chucho_finalize();
    return rc;
}
