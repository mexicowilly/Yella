#include "plugin/file/event_source.h"
#include "plugin/file/file_name_matcher.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <cjson/cJSON.h>
#include <stdlib.h>

SGLIB_DEFINE_RBTREE_FUNCTIONS(event_source_spec, left, right, color, EVENT_SOURCE_SPEC_COMPARATOR);

static void destroy_event_source_spec(event_source_spec* spec)
{
    udsfree(spec->name);
    yella_destroy_ptr_vector(spec->includes);
    yella_destroy_ptr_vector(spec->excludes);
    free(spec);
}

static char* spec_to_json(const event_source_spec* const spec)
{
    cJSON* json;
    char* utf8;
    cJSON* arr;
    size_t i;
    char* result;

    json = cJSON_CreateObject();
    utf8 = yella_to_utf8(spec->name);
    cJSON_AddItemToObject(json, "name", cJSON_CreateString(utf8));
    free(utf8);
    arr = cJSON_CreateArray();
    for (i = 0; i < yella_ptr_vector_size(spec->includes); i++)
    {
        utf8 = yella_to_utf8((UChar*)yella_ptr_vector_at(spec->includes, i));
        cJSON_AddItemToArray(arr, cJSON_CreateString(utf8));
        free(utf8);
    }
    cJSON_AddItemToObject(json, "includes", arr);
    arr = cJSON_CreateArray();
    for (i = 0; i < yella_ptr_vector_size(spec->excludes); i++)
    {
        utf8 = yella_to_utf8((UChar*)yella_ptr_vector_at(spec->excludes, i));
        cJSON_AddItemToArray(arr, cJSON_CreateString(utf8));
        free(utf8);
    }
    cJSON_AddItemToObject(json, "excludes", arr);
    result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return result;
}

void add_or_replace_event_source_spec(event_source* esrc, event_source_spec* spec)
{
    event_source_spec* removed;
    char* spec_text;

    yella_write_lock_reader_writer_lock(esrc->guard);
    if (sglib_event_source_spec_delete_if_member(&esrc->specs, spec, &removed) != 0)
        destroy_event_source_spec(removed);
    sglib_event_source_spec_add(&esrc->specs, spec);
    add_or_replace_event_source_impl_spec(esrc, spec);
    yella_unlock_reader_writer_lock(esrc->guard);
    if (chucho_logger_permits(esrc->lgr, CHUCHO_INFO))
    {
        spec_text = spec_to_json(spec);
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
    result->lgr = chucho_get_logger("yella.file.event");
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
    clear_event_source_impl_specs(esrc);
    yella_unlock_reader_writer_lock(esrc->guard);
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
    remove_event_source_impl_spec(esrc, name);
    yella_unlock_reader_writer_lock(esrc->guard);
    if (chucho_logger_permits(esrc->lgr, CHUCHO_INFO))
    {
        spec_text = yella_to_utf8(name);
        CHUCHO_C_INFO(esrc->lgr, "Removed config: %s", spec_text);
        free(spec_text);
    }
}
