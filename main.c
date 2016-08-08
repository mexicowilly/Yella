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

#include "common/log.h"
#include "common/settings.h"
#include <chucho/configuration.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void process_command_line(int argc, char* argv[])
{
    int i;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--config-file") == 0)
        {
            if (++i == argc)
            {
                printf("The option --config-file requires an argument\n");
                exit(EXIT_FAILURE);
            }
            yella_settings_set_text("config-file", argv[i]);
        }
    }
}

int main(int argc, char* argv[])
{
    yella_initialize_settings();
    process_command_line(argc, argv);
    chucho_cnf_set_file_name(yella_settings_get_text("config-file"));
    CHUCHO_C_INFO(yella_logger("yella"), "Yella is starting");
    yella_destroy_settings();
    yella_destroy_loggers();
    return EXIT_SUCCESS;
}
