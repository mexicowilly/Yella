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
#include "uds.h"
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
    uds key;
    union
    {
        uds text;
        uint64_t uint;
    } value;
    yella_setting_value_type type;
    char color;
    struct setting* left;
    struct setting* right;
} setting;

typedef struct section
{
    uds key;
    setting* settings;
    char color;
    struct section* left;
    struct section* right;
} section;

#define YELLA_KEY_COMPARE(x, y) u_strcmp((x->key), (y->key))

SGLIB_DEFINE_RBTREE_PROTOTYPES(setting, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(setting, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_PROTOTYPES(section, left, right, color, YELLA_KEY_COMPARE);
SGLIB_DEFINE_RBTREE_FUNCTIONS(section, left, right, color, YELLA_KEY_COMPARE);

static section* sections = NULL;
static yaml_document_t* yaml_doc = NULL;
static yella_mutex* guard = NULL;
static chucho_logger_t* lgr = NULL;

static void handle_yaml_node(const yaml_node_t* node,
                             const UChar* const section,
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
    UChar* key;
    UChar* value;

    if (node->type == YAML_SCALAR_NODE)
    {
        if (key_desc != NULL)
        {
            if (key_desc->type == YELLA_SETTING_VALUE_TEXT)
            {
                value = yella_from_utf8((const char*)node->data.scalar.value);
                yella_settings_set_text(section, key_desc->key, value);
                free(value);
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
                key = yella_from_utf8((const char*)key_node->data.scalar.value);
                if (u_strcmp(key, section) == 0)
                {
                    free(key);
                    handle_yaml_node(yaml_document_get_node(yaml_doc, pr->value), section, NULL, all_desc, all_desc_count);
                }
                else
                {
                    found = false;
                    for (index = 0; index < all_desc_count; index++)
                    {
                        if (u_strcmp(all_desc[index].key, key) == 0)
                        {
                            found = true;
                            break;
                        }
                    }
                    free(key);
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

static const void* get_value(const UChar* const sct, const UChar* const key, yella_setting_value_type type)
{
    setting set_to_find;
    setting* set_found;
    section sct_to_find;
    section* sct_found;
    char* utf8;

    sct_to_find.key = (UChar*)sct;
    sct_found = sglib_section_find_member(sections, &sct_to_find);
    if (sct_found == NULL)
    {
        utf8 = yella_to_utf8(sct);
        CHUCHO_C_ERROR_L(lgr,
                         "The section %s was not found",
                         utf8);
        free(utf8);
        return NULL;
    }
    else
    {
        set_to_find.key = (UChar*)key;
        set_found = sglib_setting_find_member(sct_found->settings, &set_to_find);
        if (set_found == NULL)
        {
            utf8 = yella_to_utf8(key);
            CHUCHO_C_ERROR_L(lgr,
                             "The setting, \"%s\", was not found",
                             utf8);
            free(utf8);
            return NULL;
        }
        else if (set_found->type != type)
        {
            utf8 = yella_to_utf8(key);
            CHUCHO_C_ERROR_L(lgr,
                             "The setting, \"%s\", is not of type, \"%s\"",
                             utf8,
                             (type == YELLA_SETTING_VALUE_TEXT ? "text" : "uint"));
            free(utf8);
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
            udsfree(set_elem->key);
            if (set_elem->type == YELLA_SETTING_VALUE_TEXT)
                udsfree(set_elem->value.text);
            free(set_elem);
        }
        udsfree(sct_elem->key);
        free(sct_elem);
    }
    sections = NULL;
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
    const UChar* file_name;
    char* fn_utf8;

    /* Get the logger here, because the config location has been
     * set properly by now. */
    lgr = chucho_get_logger("yella.settings");
    file_name = yella_settings_get_text(u"agent", u"config-file");
    if (file_name == NULL)
    {
        CHUCHO_C_ERROR_L(lgr,
                         "You must set the config-file setting before calling yaml_load_settings_doc");
        return YELLA_LOGIC_ERROR;
    }
    if (!yella_file_exists(file_name))
    {
        fn_utf8 = yella_to_utf8(file_name);
        CHUCHO_C_ERROR_L(lgr,
                         "The configuration file %s does not exist",
                         fn_utf8);
        free(fn_utf8);
        return YELLA_DOES_NOT_EXIST;
    }
    yella_file_size(file_name, &sz);
    if (sz > 100 * 1024)
    {
        fn_utf8 = yella_to_utf8(file_name);
        CHUCHO_C_ERROR_L(lgr,
                         "The configuration file %s has a size of %llu, which is greater than the maximum allowed of 100 KB",
                         fn_utf8,
                         sz);
        free(fn_utf8);
        return YELLA_TOO_BIG;
    }
    fn_utf8 = yella_to_utf8(file_name);
    f = fopen(fn_utf8, "r");
    if (f == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR_L(lgr,
                         "Unable to open the config file %s for reading: %s",
                         fn_utf8,
                         strerror(err));
        free(fn_utf8);
        return YELLA_READ_ERROR;
    }
    free(fn_utf8);
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
    uds out;
    char* utf8;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        out = udscatprintf(udsempty(), u"Settinngs:%S%S", yella_nl, yella_nl);
        yella_lock_mutex(guard);
        for (sct_elem = sglib_section_it_init(&sct_itor, sections);
             sct_elem != NULL;
             sct_elem = sglib_section_it_next(&sct_itor))
        {
            udscatuds(out, sct_elem->key);
            udscatlen(out, ":", 1);
            udscat(out, yella_nl);
            for (set_elem = sglib_setting_it_init(&set_itor, sct_elem->settings);
                 set_elem != NULL;
                 set_elem = sglib_setting_it_next(&set_itor))
            {
                udscatlen(out, "  ", 2);
                udscatuds(out, set_elem->key);
                udscatlen(out, "=", 1);
                if (set_elem->type == YELLA_SETTING_VALUE_TEXT)
                {
                    udscatlen(out, "'", 1);
                    udscatuds(out, set_elem->value.text);
                    udscatlen(out, "'", 1);
                }
                else
                {
                    udscatprintf(out, u"%lld", set_elem->value.uint);
                }
                udscat(out, yella_nl);
            }
        }
        yella_unlock_mutex(guard);
        udstrim(out, u"\n\r");
        utf8 = yella_to_utf8(out);
        CHUCHO_C_INFO_L(lgr, "%s", utf8);
        free(utf8);
        udsfree(out);
    }
}

void yella_retrieve_settings(const UChar* const section, const yella_setting_desc* desc, size_t count)
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

const uint64_t* yella_settings_get_uint(const UChar* const sct, const UChar* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_UINT);
}

const UChar* yella_settings_get_text(const UChar* const sct, const UChar* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_TEXT);
}

void yella_settings_set_uint(const UChar* const sct, const UChar* const key, uint64_t val)
{
    setting set_to_find;
    setting* set_found;
    section sct_to_find;
    section* sct_found;

    sct_to_find.key = (UChar*)sct;
    sct_found = sglib_section_find_member(sections, &sct_to_find);
    if (sct_found == NULL)
    {
        sct_found = malloc(sizeof(section));
        sct_found->key = udsnew(sct);
        sct_found->settings = NULL;
        sglib_section_add(&sections, sct_found);
    }
    set_to_find.key = (UChar*)key;
    set_found = sglib_setting_find_member(sct_found->settings, &set_to_find);
    if (set_found == NULL)
    {
        set_found = malloc(sizeof(setting));
        set_found->key = udsnew(key);
        set_found->value.uint = val;
        set_found->type = YELLA_SETTING_VALUE_UINT;
        sglib_setting_add(&sct_found->settings, set_found);
    }
    else
    {
        set_found->value.uint = val;
    }
}

void yella_settings_set_text(const UChar* const sct, const UChar* const key, const UChar* const val)
{
    setting set_to_find;
    setting* set_found;
    section sct_to_find;
    section* sct_found;

    sct_to_find.key = (UChar*)sct;
    sct_found = sglib_section_find_member(sections, &sct_to_find);
    if (sct_found == NULL)
    {
        sct_found = malloc(sizeof(section));
        sct_found->key = udsnew(sct);
        sct_found->settings = NULL;
        sglib_section_add(&sections, sct_found);
    }
    set_to_find.key = (UChar*)key;
    set_found = sglib_setting_find_member(sct_found->settings, &set_to_find);
    if (set_found == NULL)
    {
        set_found = malloc(sizeof(setting));
        set_found->key = udsnew(key);
        set_found->value.text = udsnew(val);
        set_found->type = YELLA_SETTING_VALUE_TEXT;
        sglib_setting_add(&sct_found->settings, set_found);
    }
    else
    {
        udsfree(set_found->value.text);
        set_found->value.text = udsnew(val);
    }
}
