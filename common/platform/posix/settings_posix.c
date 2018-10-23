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
#include "common/text_util.h"
#include "common/uds.h"
#include <stdlib.h>

void yella_initialize_platform_settings(void)
{
    uds plg;
    UChar* utf16;

    yella_settings_set_text(u"agent", u"config-file", u"/etc/yella.yaml");
    yella_settings_set_text(u"agent", u"data-dir", u"/var/lib/yella");
    yella_settings_set_text(u"agent", u"spool-dir", u"/var/spool/yella");
    utf16 = yella_from_utf8(YELLA_VALUE_STR(YELLA_INSTALL_PREFIX));
    plg = udsnew(utf16);
    free(utf16);
    plg = udscat(plg, u"/plugin");
    yella_settings_set_text(u"agent", u"plugin-dir", plg);
    udsfree(plg);
}
