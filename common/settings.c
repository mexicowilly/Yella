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
#include "sds.h"
#include "file.h"
#include "sglib.h"
#include "thread.h"
#include <chucho/log.h>
#include <yaml.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

void yella_initialize_platform_settings(void);

typedef struct setting
{
    sds key;
    union
    {
        sds text;
        uint64_t uint;
    } value;
    yella_setting_value_type type;
    char color;
    struct setting* left;
    struct setting* right;
} setting;

typedef struct section
{
    sds key;
    setting* settings;
    char color;
    struct section* left;
    struct section* right;
} section;

#define YELLA_KEY_COMPARE(x, y) strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(setting, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(setting, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_PROTOTYPES(section, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(section, left, right, color, YELLA_KEY_COMPARE);

static section* sections = NULL;
static yaml_document_t* yaml_doc = NULL;
static yella_mutex* guard = NULL;
static chucho_logger_t* lgr = NULL;

static void handle_yaml_node(const yaml_node_t* node,
                             const char* const section,
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
                yella_settings_set_text(section, key_desc->key, (const char*)node->data.scalar.value);
            }
            else if (key_desc->type == YELLA_SETTING_VALUE_UINT)
            {
                yella_settings_set_uint(section, key_desc->key, strtoull((const char*)node->data.scalar.value, NULL, 10));
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
                if (strcmp((const char*)key_node->data.scalar.value, section) == 0)
                {
                    handle_yaml_node(yaml_document_get_node(yaml_doc, pr->value), section, NULL, all_desc, all_desc_count);
                }
                else
                {
                    found = false;
                    for (index = 0; index < all_desc_count; index++)
                    {
                        if (strcmp(all_desc[index].key, (const char*)key_node->data.scalar.value) == 0)
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
                                             section,
                                             &all_desc[index],
                                             all_desc,
                                             all_desc_count);
                        }
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
                             section,
                             NULL,
                             all_desc,
                             all_desc_count);
        }
    }
}

static const void* get_value(const char* const sct, const char* const key, yella_setting_value_type type)
{
    setting set_to_find;
    setting* set_found;
    section sct_to_find;
    section* sct_found;

    sct_to_find.key = (char*)sct;
    sct_found = sglib_section_find_member(sections, &sct_to_find);
    if (sct_found == NULL)
    {
        CHUCHO_C_ERROR_L(lgr,
                         "The section %s was not found",
                         sct);
        return NULL;
    }
    else
    {
        set_to_find.key = (char*)key;
        set_found = sglib_setting_find_member(sct_found->settings, &set_to_find);
        if (set_found == NULL)
        {
            CHUCHO_C_ERROR_L(lgr,
                             "The setting, \"%s\", was not found",
                             key);
            return NULL;
        }
        else if (set_found->type != type)
        {
            CHUCHO_C_ERROR_L(lgr,
                             "The setting, \"%s\", is not of type, \"%s\"",
                             key,
                             (type == YELLA_SETTING_VALUE_TEXT ? "text" : "uint"));
            return NULL;
        }
    }
    if (type == YELLA_SETTING_VALUE_TEXT)
        return set_found->value.text;
    else
        return &set_found->value.uint;
}

void yella_destroy_settings(void)
{
    section* sct_elem;
    setting* set_elem;
    struct sglib_setting_iterator set_itor;
    struct sglib_section_iterator sct_itor;

    for (sct_elem = sglib_section_it_init(&sct_itor, sections);
         sct_elem != NULL;
         sct_elem = sglib_section_it_next(&sct_itor))
    {
        for (set_elem = sglib_setting_it_init(&set_itor, sct_elem->settings);
             set_elem != NULL;
             set_elem = sglib_setting_it_next(&set_itor))
        {
            sdsfree(set_elem->key);
            if (set_elem->type == YELLA_SETTING_VALUE_TEXT)
                sdsfree(set_elem->value.text);
            free(set_elem);
        }
        sdsfree(sct_elem->key);
        free(sct_elem);
    }
    yella_destroy_mutex(guard);
    chucho_release_logger(lgr);
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
    size_t sz = 0;
    const char* file_name;

    /* Get the logger here, because the config location has been
     * set properly by now. */
    lgr = chucho_get_logger("yella.settings");
    file_name = yella_settings_get_text("agent", "config-file");
    if (file_name == NULL)
    {
        CHUCHO_C_ERROR_L(lgr,
                         "You must set the config-file setting before calling yaml_load_settings_doc");
        return YELLA_LOGIC_ERROR;
    }
    if (!yella_file_exists(file_name))
    {
        CHUCHO_C_ERROR_L(lgr,
                         "The configuration file %s does not exist",
                         file_name);
        return YELLA_DOES_NOT_EXIST;
    }
    yella_file_size(file_name, &sz);
    if (sz > 100 * 1024)
    {
        CHUCHO_C_ERROR_L(lgr,
                         "The configuration file %s has a size of %llu, which is greater than the maximum allowed of 100 KB",
                         file_name,
                         sz);
        return YELLA_TOO_BIG;
    }
    f = fopen(file_name, "r");
    if (f == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR_L(lgr,
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
        CHUCHO_C_ERROR_L(lgr,
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

void yella_log_settings(void)
{
    section* sct_elem;
    setting* set_elem;
    struct sglib_setting_iterator set_itor;
    struct sglib_section_iterator sct_itor;
    sds out;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        out = sdsempty();
        yella_lock_mutex(guard);
        for (sct_elem = sglib_section_it_init(&sct_itor, sections);
             sct_elem != NULL;
             sct_elem = sglib_section_it_next(&sct_itor))
        {
            sdscatsds(out, sct_elem->key);
            sdscatlen(out, ":", 1);
            sdscat(out, yella_nl);
            for (set_elem = sglib_setting_it_init(&set_itor, sct_elem->settings);
                 set_elem != NULL;
                 set_elem = sglib_setting_it_next(&set_itor))
            {
                sdscatlen(out, "  ", 2);
                sdscatsds(out, set_elem->key);
                sdscatlen(out, "=", 1);
                if (set_elem->type == YELLA_SETTING_VALUE_TEXT)
                {
                    sdscatlen(out, "'", 1);
                    sdscatsds(out, set_elem->value.text);
                    sdscatlen(out, "'", 1);
                }
                else
                {
                    sdscatprintf(out, "%" PRIu64, set_elem->value.uint);
                }
                sdscat(out, yella_nl);
            }
        }
        yella_unlock_mutex(guard);
        sdstrim(out, "\n\r");
        CHUCHO_C_INFO_L(lgr, "Settings:%s%s%s", yella_nl, yella_nl, out);
        sdsfree(out);
    }
}

void yella_retrieve_settings(const char* const section, const yella_setting_desc* desc, size_t count)
{
    yaml_node_t* node;
    yella_setting_desc* desc_copy;

    if (yaml_doc != NULL)
    {
        node = yaml_document_get_root_node(yaml_doc);
        if (node != NULL)
        {
            yella_lock_mutex(guard);
            handle_yaml_node(node, section, NULL, desc, count);
            yella_unlock_mutex(guard);
        }
    }
}

const uint64_t* yella_settings_get_uint(const char* const sct, const char* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_UINT);
}

const char* yella_settings_get_text(const char* const sct, const char* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_TEXT);
}

void yella_settings_set_uint(const char* const sct, const char* const key, uint64_t val)
{
    setting set_to_find;
    setting* set_found;
    section sct_to_find;
    section* sct_found;

    sct_to_find.key = (char*)sct;
    sct_found = sglib_section_find_member(sections, &sct_to_find);
    if (sct_found == NULL)
    {
        sct_found = malloc(sizeof(section));
        sct_found->key = sdsnew(sct);
        sct_found->settings = NULL;
        sglib_section_add(&sections, sct_found);
    }
    set_to_find.key = (char*)key;
    set_found = sglib_setting_find_member(sct_found->settings, &set_to_find);
    if (set_found == NULL)
    {
        set_found = malloc(sizeof(setting));
        set_found->key = sdsnew(key);
        set_found->value.uint = val;
        set_found->type = YELLA_SETTING_VALUE_UINT;
        sglib_setting_add(&sct_found->settings, set_found);
    }
    else
    {
        set_found->value.uint = val;
    }
}

void yella_settings_set_text(const char* const sct, const char* const key, const char* const val)
{
    setting set_to_find;
    setting* set_found;
    section sct_to_find;
    section* sct_found;

    sct_to_find.key = (char*)sct;
    sct_found = sglib_section_find_member(sections, &sct_to_find);
    if (sct_found == NULL)
    {
        sct_found = malloc(sizeof(section));
        sct_found->key = sdsnew(sct);
        sct_found->settings = NULL;
        sglib_section_add(&sections, sct_found);
    }
    set_to_find.key = (char*)key;
    set_found = sglib_setting_find_member(sct_found->settings, &set_to_find);
    if (set_found == NULL)
    {
        set_found = malloc(sizeof(setting));
        set_found->key = sdsnew(key);
        set_found->value.text = sdsnew(val);
        set_found->type = YELLA_SETTING_VALUE_TEXT;
        sglib_setting_add(&sct_found->settings, set_found);
    }
    else
    {
        sdsfree(set_found->value.text);
        set_found->value.text = sdsnew(val);
    }
}
