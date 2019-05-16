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
#include "ptr_vector.h"
#include <chucho/log.h>
#include <yaml.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <unicode/ustring.h>
#include <unicode/uchar.h>
#include <unicode/ustdio.h>

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
            if (key_desc->type == YELLA_SETTING_VALUE_TEXT || key_desc->type == YELLA_SETTING_VALUE_DIR || key_desc->type == YELLA_SETTING_VALUE_BYTE_SIZE)
            {
                value = yella_from_utf8((const char*)node->data.scalar.value);
                switch (key_desc->type)
                {
                case YELLA_SETTING_VALUE_BYTE_SIZE:
                    yella_settings_set_byte_size(section, key_desc->key, value);
                    break;
                case YELLA_SETTING_VALUE_DIR:
                    yella_settings_set_dir(section, key_desc->key, value);
                    break;
                default:
                    yella_settings_set_text(section, key_desc->key, value);
                    break;
                }
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

static const char* type_to_str(yella_setting_value_type tp)
{
    switch (tp)
    {
    case YELLA_SETTING_VALUE_TEXT:
        return "text";
    case YELLA_SETTING_VALUE_DIR:
        return "dir";
    case YELLA_SETTING_VALUE_BYTE_SIZE:
        return "byte size";
    default:
        return "uint";
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
                         "The section '%s' was not found",
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
                             "The setting, '%s', was not found",
                             utf8);
            free(utf8);
            return NULL;
        }
        else if (set_found->type != type)
        {
            utf8 = yella_to_utf8(key);
            CHUCHO_C_ERROR_L(lgr,
                             "The setting, '%s', is not of type, '%s'",
                             utf8,
                             type_to_str(type));
            free(utf8);
            return NULL;
        }
    }
    if (type == YELLA_SETTING_VALUE_UINT || type == YELLA_SETTING_VALUE_BYTE_SIZE)
        return &set_found->value.uint;
    else
        return set_found->value.text;
}

static int qsort_ustr_comparator(const void * lhs, const void * rhs)
{
    const UChar* ulhs;
    const UChar* urhs;

    ulhs = *(const UChar**)lhs;
    urhs = *(const UChar**)rhs;
    return u_strcmp(ulhs, urhs);
}

static void set_text_impl(const UChar* const sct, const UChar* const key, const UChar* const val, yella_setting_value_type type)
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
        set_found->type = type;
        sglib_setting_add(&sct_found->settings, set_found);
    }
    else
    {
        udsfree(set_found->value.text);
        set_found->value.text = udsnew(val);
    }
}

static void set_uint_impl(const UChar* const sct, const UChar* const key, uint64_t val, yella_setting_value_type type)
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
        set_found->type = type;
        sglib_setting_add(&sct_found->settings, set_found);
    }
    else
    {
        set_found->value.uint = val;
    }
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
                         "The configuration file '%s' does not exist",
                         fn_utf8);
        free(fn_utf8);
        return YELLA_DOES_NOT_EXIST;
    }
    yella_file_size(file_name, &sz);
    if (sz > 100 * 1024)
    {
        fn_utf8 = yella_to_utf8(file_name);
        CHUCHO_C_ERROR_L(lgr,
                         "The configuration file '%s' has a size of %llu, which is greater than the maximum allowed of 100 KB",
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
                         "Unable to open the config file '%s' for reading: %s",
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
    yella_ptr_vector* sct_vec;
    yella_ptr_vector* set_vec;
    size_t i;
    size_t j;
    section sct_to_find;
    setting set_to_find;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        sct_vec = yella_create_ptr_vector();
        yella_set_ptr_vector_destructor(sct_vec, NULL, NULL);
        yella_lock_mutex(guard);
        for (sct_elem = sglib_section_it_init(&sct_itor, sections);
             sct_elem != NULL;
             sct_elem = sglib_section_it_next(&sct_itor))
        {
            yella_push_back_ptr_vector(sct_vec, sct_elem->key);
        }
        qsort(yella_ptr_vector_data(sct_vec), yella_ptr_vector_size(sct_vec), sizeof(void*), qsort_ustr_comparator);
        out = udscatprintf(udsempty(), u"Settings:%S", YELLA_NL);
        for (i = 0; i < yella_ptr_vector_size(sct_vec); i++)
        {
            sct_to_find.key = yella_ptr_vector_at(sct_vec, i);
            sct_elem = sglib_section_find_member(sections, &sct_to_find);
            assert(sct_elem != NULL);
            out = udscatprintf(out, u"  %S:%S", sct_elem->key, YELLA_NL);
            set_vec = yella_create_ptr_vector();
            yella_set_ptr_vector_destructor(set_vec, NULL, NULL);
            for (set_elem = sglib_setting_it_init(&set_itor, sct_elem->settings);
                 set_elem != NULL;
                 set_elem = sglib_setting_it_next(&set_itor))
            {
                yella_push_back_ptr_vector(set_vec, set_elem->key);
            }
            qsort(yella_ptr_vector_data(set_vec), yella_ptr_vector_size(set_vec), sizeof(void*), qsort_ustr_comparator);
            for (j = 0; j < yella_ptr_vector_size(set_vec); j++)
            {
                set_to_find.key = yella_ptr_vector_at(set_vec, j);
                set_elem = sglib_setting_find_member(sct_elem->settings, &set_to_find);
                assert(set_elem != NULL);
                out = udscatprintf(out, u"    %S=", set_elem->key);
                if (set_elem->type == YELLA_SETTING_VALUE_TEXT || set_elem->type == YELLA_SETTING_VALUE_DIR)
                    out = udscatprintf(out, u"'%S'", set_elem->value.text);
                else
                    out = udscatprintf(out, u"%lld", set_elem->value.uint);
                out = udscat(out, YELLA_NL);
            }
            yella_destroy_ptr_vector(set_vec);
        }
        yella_destroy_ptr_vector(sct_vec);
        yella_unlock_mutex(guard);
        out = udstrim(out, u"\n\r");
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

const uint64_t* yella_settings_get_byte_size(const UChar* const sct, const UChar* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_BYTE_SIZE);
}

const UChar* yella_settings_get_dir(const UChar* const sct, const UChar* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_DIR);
}

const UChar* yella_settings_get_text(const UChar* const sct, const UChar* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_TEXT);
}

const uint64_t* yella_settings_get_uint(const UChar* const sct, const UChar* const key)
{
    return get_value(sct, key, YELLA_SETTING_VALUE_UINT);
}

void yella_settings_set_byte_size(const UChar* const sct, const UChar* const key, const UChar* const val)
{
    int32_t val_len;
    int32_t len;
    UChar lower[4];
    UErrorCode uerr;
    char* utf8;
    uint32_t num;
    UChar suffix[4];

    num = 0;
    val_len = u_strlen(val);
    if (val_len == 0 || !u_isdigit(val[0]))
    {
        utf8 = yella_to_utf8(val);
        CHUCHO_C_ERROR(lgr, "The value '%s' cannot be used as a byte size because it does not start with a digit", utf8);
        free(utf8);
    }
    else
    {
        memset(suffix, 0, sizeof(UChar) * 4);
        len = u_sscanf_u(val, u"%u%3S", &num, &suffix);
        if (len != 1 && len != 2)
        {
            utf8 = yella_to_utf8(val);
            CHUCHO_C_ERROR(lgr, "Invalid byte size format: '%s'", utf8);
            free(utf8);
            num = 0;
        }
        else if (len == 2)
        {
            val_len = u_strlen(suffix);
            uerr = U_ZERO_ERROR;
            len = u_strToLower(lower, 4, suffix, val_len, NULL, &uerr);
            if (uerr != U_ZERO_ERROR)
            {
                utf8 = yella_to_utf8(val);
                CHUCHO_C_ERROR(lgr, "Error converting '%s' to lowercase: %s", utf8, u_errorName(uerr));
                free(utf8);
            }
            else
            {
                if (len > 2 ||
                    (len == 2 && lower[1] != u'b') ||
                    (len == 2 && lower[0] == u'b') ||
                    (u_strchr(u"bkmg", lower[0]) == NULL))
                {
                    utf8 = yella_to_utf8(val);
                    CHUCHO_C_ERROR(lgr, "The suffix of value '%s' is invalid (case-insensitive b, k[b], m[b], g[b])", utf8);
                    free(utf8);
                    num = 0;
                }
                else
                {
                    switch (lower[0])
                    {
                    case u'b':
                        break;
                    case u'k':
                        num *= 1024;
                        break;
                    case u'm':
                        num *= 1024 * 1024;
                        break;
                    case u'g':
                        num *= 1024 * 1024 * 1024;
                        break;
                    }

                }
            }
        }
    }
    if (num > 0)
        set_uint_impl(sct, key, num, YELLA_SETTING_VALUE_BYTE_SIZE);
}

void yella_settings_set_dir(const UChar* const sct, const UChar* const key, const UChar* const val)
{
    uds actual;
    int32_t len;

    actual = yella_remove_duplicate_dir_seps(val);
    len = u_strlen(actual);
    if (len > 0 && actual[len - 1] != YELLA_DIR_SEP[0])
        actual = udscatlen(actual, &YELLA_DIR_SEP[0], 1);
    set_text_impl(sct, key, actual, YELLA_SETTING_VALUE_DIR);
}

void yella_settings_set_text(const UChar* const sct, const UChar* const key, const UChar* const val)
{
    set_text_impl(sct, key, val, YELLA_SETTING_VALUE_TEXT);
}

void yella_settings_set_uint(const UChar* const sct, const UChar* const key, uint64_t val)
{
    set_uint_impl(sct, key, val, YELLA_SETTING_VALUE_UINT);
}

