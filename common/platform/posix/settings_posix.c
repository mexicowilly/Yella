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

#define XSTR(s) STR(s)
#define STR(s) #s

void yella_initialize_platform_settings(void)
{
    yella_settings_set_text("config-file", "/etc/yella.yaml");
    yella_settings_set_text("log-dir", "/var/log/yella");
    yella_settings_set_text("data-dir", "/var/lib/yella");
    yella_settings_set_text("spool-dir", "/var/spool/yella");
    yella_settings_set_text("plugin-dir", XSTR(YELLA_INSTALL_PREFIX) "/lib");
}
