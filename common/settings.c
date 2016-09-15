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
#include "file.h"
#include "sglib.h"
#include "thread.h"
#include <yaml.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

void yella_initialize_platform_settings(void);

typedef struct setting
{
    char* key;
    union
    {
        char* text;
        uint64_t uint;
    } value;
    yella_setting_value_type type;
    char color;
    struct setting* left;
    struct setting* right;
} setting;

#define YELLA_SETTING_COMPARE(x, y) strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(setting, left, right, color, YELLA_SETTING_COMPARE);

SGLIB_DEFINE_RBTREE_FUNCTIONS(setting, left, right, color, YELLA_SETTING_COMPARE);

static setting* settings = NULL;
static yaml_document_t* yaml_doc = NULL;
static yella_mutex* guard = NULL;

static void handle_yaml_node(const yaml_node_t* node,
                             const yella_setting_desc* key_desc,
                             const yella_setting_desc* all_desc,
                             size_t all_desc_count)
{
    yaml_node_pair_t* pr;
    yaml_node_t* key_node;
    bool found;
    yaml_node_t* val_node;
    yaml_node_item_t* seq_item;
    int index;

    if (node->type == YAML_SCALAR_NODE)
    {
        if (key_desc != NULL)
        {
            if (key_desc->type == YELLA_SETTING_VALUE_TEXT)
            {
                yella_settings_set_text(key_desc->key, node->data.scalar.value); 
            }
            else if (key_desc->type == YELLA_SETTING_VALUE_UINT)
            {
                yella_settings_set_uint(key_desc->key, yella_text_to_int(node->data.scalar.value));
            }
        }
    }
    else if (node->type == YAML_MAPPING_NODE)
    {
        for(pr = node->data.mapping.pairs.start;
            pr < node->data.mapping.pairs.top;
            pr++)
        {
            key_node = yaml_document_get_node(yaml_doc, pr->key);
            if (key_node->type == YAML_SCALAR_NODE)
            {
                found = false;
                for (index = 0; index < all_desc_count; index++)
                {
                    if (strcmp(all_desc[index].key, key_node->data.scalar.value) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    val_node = yaml_document_get_node(yaml_doc, pr->value);
                    if (val_node->type == YAML_SCALAR_NODE)
                    {
                        handle_yaml_node(val_node,
                                         &all_desc[index],
                                         all_desc,
                                         all_desc_count);
                    }
                }
            }
        }
    }
    else if (node->type == YAML_SEQUENCE_NODE)
    {
        for (seq_item = node->data.sequence.items.start;
             seq_item < node->data.sequence.items.top;
             seq_item++)
        {
            handle_yaml_node(yaml_document_get_node(yaml_doc, *seq_item),
                             NULL,
                             all_desc,
                             all_desc_count);
        }
    }
}

void yella_destroy_settings(void)
{
    setting* elem;
    struct sglib_setting_iterator itor;

    for (elem = sglib_setting_it_init(&itor, settings);
         elem != NULL;
         elem = sglib_setting_it_next(&itor))
    {
        free(elem->key);
        if (elem->type == YELLA_SETTING_VALUE_TEXT)
            free(elem->value.text);
        free(elem);
    }
    yella_destroy_mutex(guard);
}

void yella_destroy_settings_doc(void)
{
    if (yaml_doc != NULL)
    {
        yaml_document_delete(yaml_doc);
        free(yaml_doc);
        yaml_doc = NULL;
    }
}

void yella_initialize_settings(void)
{
    guard = yella_create_mutex();
    yella_initialize_platform_settings();
}

yella_rc yella_load_settings_doc(void)
{
    yaml_parser_t parser;
    FILE* f;
    int err;
    uint64_t sz;
    const char* file_name;

    file_name = yella_settings_get_text("config-file");
    if (file_name == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "You must set the config-file setting before calling yaml_load_settings_doc");
        return YELLA_LOGIC_ERROR;
    }
    if (!yella_file_exists(file_name))
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The configuration file %s does not exist",
                       file_name);
        return YELLA_DOES_NOT_EXIST;
    }
    sz = yella_file_size(file_name);
    if (sz > 100 * 1024)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The configuration file %s has a size of %llu, which is greater than the maximum allowed of 100 KB",
                       file_name,
                       sz);
        return YELLA_TOO_BIG;
    }
    f = fopen(file_name, "r");
    if (f == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "Unable to open the config file %s for reading: %s",
                       file_name,
                       strerror(err));
        return YELLA_READ_ERROR;
    }
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);
    yaml_doc = malloc(sizeof(yaml_document_t));
    if (!yaml_parser_load(&parser, yaml_doc))
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "YAML error [%u, %u]: %s",
                       parser.mark.line,
                       parser.mark.column,
                       parser.problem);
        free(yaml_doc);
        yaml_doc = NULL;
        return YELLA_INVALID_FORMAT;
    }
    yaml_parser_delete(&parser);
    fclose(f);
    return YELLA_NO_ERROR;
}

void yella_retrieve_settings(const yella_setting_desc* desc, size_t count)
{
    yaml_node_t* node;
    yella_setting_desc* desc_copy;

    if (yaml_doc != NULL)
    {
        node = yaml_document_get_root_node(yaml_doc);
        if (node != NULL)
        {
            yella_lock_mutex(guard);
            handle_yaml_node(node, NULL, desc, count);
            yella_unlock_mutex(guard);
        }
    }
}

const uint64_t* yella_settings_get_uint(const char* const key)
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
    else if(found->type != YELLA_SETTING_VALUE_UINT)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The setting %s is not of type uint",
                       key);
        return NULL;
    }
    return &found->value.uint;
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
    else if(found->type != YELLA_SETTING_VALUE_TEXT)
    {
        CHUCHO_C_ERROR(yella_logger("yella.common"),
                       "The setting %s is not of type text",
                       key);
        return NULL;
    }
    return found->value.text;
}

void yella_settings_set_uint(const char* const key, uint64_t val)
{
    setting to_find;
    setting* found;

    to_find.key = (char*)key;
    found = sglib_setting_find_member(settings, &to_find);
    if (found == NULL)
    {
        found = malloc(sizeof(setting));
        found->key = yella_text_dup(key);
        found->value.uint = val;
        found->type = YELLA_SETTING_VALUE_UINT;
        sglib_setting_add(&settings, found);
    }
    else
    {
        found->value.uint = val;
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
        found->key = yella_text_dup(key);
        found->value.text = yella_text_dup(val);
        found->type = YELLA_SETTING_VALUE_TEXT;
        sglib_setting_add(&settings, found);
    }
    else
    {
        free(found->value.text);
        found->value.text = yella_text_dup(val);
    }
}
