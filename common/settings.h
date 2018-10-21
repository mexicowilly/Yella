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

#if !defined(SETTINGS_H__)
#define SETTINGS_H__

#include "export.h"
#include "common/return_code.h"
#include <unicode/utypes.h>
#include <string.h>
#include <stdint.h>

typedef enum yella_setting_value_type
{
    YELLA_SETTING_VALUE_TEXT,
    YELLA_SETTING_VALUE_UINT
} yella_setting_value_type;

typedef struct yella_setting_desc
{
    const UChar* key;
    yella_setting_value_type type;
} yella_setting_desc;

YELLA_EXPORT void yella_destroy_settings(void);
YELLA_EXPORT void yella_destroy_settings_doc(void);
YELLA_EXPORT void yella_initialize_settings(void);
YELLA_EXPORT yella_rc yella_load_settings_doc(void);
YELLA_EXPORT void yella_log_settings(void);
YELLA_EXPORT void yella_retrieve_settings(const UChar* const section, const yella_setting_desc* desc, size_t count);
YELLA_EXPORT const uint64_t* yella_settings_get_uint(const UChar* const section, const UChar* const key);
YELLA_EXPORT const UChar* yella_settings_get_text(const UChar* const section, const UChar* const key);
YELLA_EXPORT void yella_settings_set_uint(const UChar* const section, const UChar* const key, uint64_t val);
YELLA_EXPORT void yella_settings_set_text(const UChar* const section, const UChar* const key, const UChar* const val);

#endif
