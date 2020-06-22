#include "plugin/file/event_source.h"
#include "plugin/file/file_name_matcher.h"
#include "common/text_util.h"
#include "common/yaml_util.h"
#include <chucho/log.h>
#include <stdlib.h>

SGLIB_DEFINE_RBTREE_FUNCTIONS(event_source_spec, left, right, color, EVENT_SOURCE_SPEC_COMPARATOR);

static void destroy_event_source_spec(event_source_spec* spec)
{
    udsfree(spec->name);
    yella_destroy_ptr_vector(spec->includes);
    yella_destroy_ptr_vector(spec->excludes);
    free(spec);
}

static char* specs_to_yaml(event_source_spec** specs, size_t count)
{
    char* utf8;
    size_t i;
    size_t j;
    yaml_document_t doc;
    int top;
    int key;
    int value;
    int cur;
    int seq;

    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    top = yaml_document_add_mapping(&doc, NULL, YAML_FLOW_MAPPING_STYLE);
    for (j = 0; j < count; j++)
    {
        cur = yaml_document_add_mapping(&doc, NULL, YAML_FLOW_MAPPING_STYLE);
        key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t*)"includes", 8, YAML_PLAIN_SCALAR_STYLE);
        seq = yaml_document_add_sequence(&doc, NULL, YAML_FLOW_SEQUENCE_STYLE);
        for (i = 0; i < yella_ptr_vector_size(specs[j]->includes); i++)
        {
            utf8 = yella_to_utf8((UChar*)yella_ptr_vector_at(specs[j]->includes, i));
            value = yaml_document_add_scalar(&doc, NULL, (yaml_char_t*)utf8, strlen(utf8), YAML_PLAIN_SCALAR_STYLE);
            free(utf8);
            yaml_document_append_sequence_item(&doc, seq, value);
        }
        yaml_document_append_mapping_pair(&doc, cur, key, seq);
        key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t*)"includes", 8, YAML_PLAIN_SCALAR_STYLE);
        seq = yaml_document_add_sequence(&doc, NULL, YAML_FLOW_SEQUENCE_STYLE);
        for (i = 0; i < yella_ptr_vector_size(specs[j]->excludes); i++)
        {
            utf8 = yella_to_utf8((UChar*)yella_ptr_vector_at(specs[j]->excludes, i));
            value = yaml_document_add_scalar(&doc, NULL, (yaml_char_t*)utf8, strlen(utf8), YAML_PLAIN_SCALAR_STYLE);
            free(utf8);
            yaml_document_append_sequence_item(&doc, seq, value);
        }
        yaml_document_append_mapping_pair(&doc, cur, key, seq);
        utf8 = yella_to_utf8(specs[j]->name);
        key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t*)utf8, strlen(utf8), YAML_PLAIN_SCALAR_STYLE);
        free(utf8);
        yaml_document_append_mapping_pair(&doc, top, key, cur);
    }
    return yella_emit_yaml(&doc);
}

void add_or_replace_event_source_specs(event_source* esrc, event_source_spec** specs, size_t count)
{
    event_source_spec* removed;
    char* spec_text;
    size_t i;

    yella_write_lock_reader_writer_lock(esrc->guard);
    for (i = 0; i < count; i++)
    {
        if (sglib_event_source_spec_delete_if_member(&esrc->specs, specs[i], &removed) != 0)
            destroy_event_source_spec(removed);
        sglib_event_source_spec_add(&esrc->specs, specs[i]);
    }
    yella_unlock_reader_writer_lock(esrc->guard);
    add_or_replace_event_source_impl_specs(esrc, specs, count);
    if (chucho_logger_permits(esrc->lgr, CHUCHO_INFO))
    {
        spec_text = specs_to_yaml(specs, count);
        CHUCHO_C_INFO(esrc->lgr, "Added config: %s", spec_text);
        free(spec_text);
    }
}

event_source* create_event_source(event_source_callback cb, void* cb_udata)
{
    event_source* result;

    result = calloc(1, sizeof(event_source));
    result->guard = yella_create_reader_writer_lock();
    result->callback = cb;
    result->callback_udata = cb_udata;
    result->lgr = chucho_get_logger("file.event");
    init_event_source_impl(result);
    return result;
}

void clear_event_source_specs(event_source* esrc)
{
    event_source_spec* cur;
    struct sglib_event_source_spec_iterator itor;

    yella_write_lock_reader_writer_lock(esrc->guard);
    for (cur = sglib_event_source_spec_it_init(&itor, esrc->specs);
         cur != NULL;
         cur = sglib_event_source_spec_it_next(&itor))
    {
        destroy_event_source_spec(cur);
    }
    esrc->specs = NULL;
    yella_unlock_reader_writer_lock(esrc->guard);
    clear_event_source_impl_specs(esrc);
    CHUCHO_C_INFO(esrc->lgr, "Cleared all configs");
}

void destroy_event_source(event_source* esrc)
{
    destroy_event_source_impl(esrc);
    clear_event_source_specs(esrc);
    yella_destroy_reader_writer_lock(esrc->guard);
    chucho_release_logger(esrc->lgr);
    free(esrc);
}

const UChar* event_source_file_name_matches_any(const event_source* const esrc, const UChar* const fname)
{
    int i;
    struct sglib_event_source_spec_iterator spec_itor;
    event_source_spec* cur_spec;
    const UChar* result;

    result = NULL;
    yella_read_lock_reader_writer_lock(esrc->guard);
    for (cur_spec = sglib_event_source_spec_it_init(&spec_itor, esrc->specs);
         cur_spec != NULL;
         cur_spec = sglib_event_source_spec_it_next(&spec_itor))
    {
        for (i = 0; i < yella_ptr_vector_size(cur_spec->excludes); i++)
        {
            if (file_name_matches(fname, yella_ptr_vector_at(cur_spec->excludes, i)))
                goto top_break;
        }
        for (i = 0; i < yella_ptr_vector_size(cur_spec->includes); i++)
        {
            if (file_name_matches(fname, yella_ptr_vector_at(cur_spec->includes, i)))
            {
                result = cur_spec->name;
                goto top_break;
            }
        }
    }
    top_break:
    yella_unlock_reader_writer_lock(esrc->guard);
    return result;
}

void remove_event_source_spec(event_source* esrc, const UChar* const name)
{
    event_source_spec to_remove;
    event_source_spec* removed;
    char* spec_text;

    to_remove.name = (UChar*)name;
    yella_write_lock_reader_writer_lock(esrc->guard);
    if (sglib_event_source_spec_delete_if_member(&esrc->specs, &to_remove, &removed))
        destroy_event_source_spec(removed);
    yella_unlock_reader_writer_lock(esrc->guard);
    remove_event_source_impl_spec(esrc, name);
    if (chucho_logger_permits(esrc->lgr, CHUCHO_INFO))
    {
        spec_text = yella_to_utf8(name);
        CHUCHO_C_INFO(esrc->lgr, "Removed config: %s", spec_text);
        free(spec_text);
    }
}
