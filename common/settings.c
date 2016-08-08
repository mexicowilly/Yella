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

#include "settings.h"
#include "text_util.h"
#include "log.h"
#include "sglib.h"
#include <stdlib.h>

typedef enum value_type
{
    YELLA_VALUE_TEXT,
    YELLA_VALUE_UINT32
} value_type;

typedef struct setting
{
    char* key;
    union
    {
        char* text;
        uint32_t uint32;
    } value;
    value_type type;
    char color;
    struct setting* left;
    struct setting* right;
} setting;

#define YELLA_SETTING_COMPARE(x, y) strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(setting, left, right, color, YELLA_SETTING_COMPARE);

SGLIB_DEFINE_RBTREE_FUNCTIONS(setting, left, right, color, YELLA_SETTING_COMPARE);

static setting* settings = NULL;

void yella_destroy_settings(void)
{
    setting* elem;
    struct sglib_setting_iterator itor;

    for (elem = sglib_setting_it_init(&itor, settings);
         elem != NULL;
         elem = sglib_setting_it_next(&itor))
    {
        free(elem->key);
        if (elem->type == YELLA_VALUE_TEXT)
            free(elem->value.text);
        free(elem);
    }
}

const uint32_t* yella_settings_get_uint32(const char* const key)
{
    setting to_find;
    setting* found;

    to_find.key = (char*)key;
    found = sglib_setting_find_member(settings, &to_find);
    if (found == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The setting %s was not found",
                       key);
        return NULL;
    }
    else if(found->type != YELLA_VALUE_UINT32)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The setting %s is not of type uint32_t",
                       key);
        return NULL;
    }
    return &found->value.uint32;
}

const char* yella_settings_get_text(const char* const key)
{
    setting to_find;
    setting* found;

    to_find.key = (char*)key;
    found = sglib_setting_find_member(settings, &to_find);
    if (found == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The setting %s was not found",
                       key);
        return NULL;
    }
    else if(found->type != YELLA_VALUE_TEXT)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The setting %s is not of type text",
                       key);
        return NULL;
    }
    return found->value.text;
}

void yella_initialize_settings(void)
{
    yella_initialize_platform_settings();
}

void yella_settings_set_uint32(const char* const key, uint32_t val)
{
    setting to_find;
    setting* found;

    to_find.key = (char*)key;
    found = sglib_setting_find_member(settings, &to_find);
    if (found == NULL)
    {
        found = malloc(sizeof(setting));
        found->key = text_dup(key);
        found->value.uint32 = val;
        found->type = YELLA_VALUE_UINT32;
        sglib_setting_add(&settings, found);
    }
    else
    {
        found->value.uint32 = val;
    }
}

void yella_settings_set_text(const char* const key, const char* const val)
{
    setting to_find;
    setting* found;

    to_find.key = (char*)key;
    found = sglib_setting_find_member(settings, &to_find);
    if (found == NULL)
    {
        found = malloc(sizeof(setting));
        found->key = text_dup(key);
        found->value.text = text_dup(val);
        found->type = YELLA_VALUE_TEXT;
        sglib_setting_add(&settings, found);
    }
    else
    {
        free(found->value.text);
        found->value.text = text_dup(val);
    }
}
