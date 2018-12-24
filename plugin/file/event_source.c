#include "plugin/file/event_source.h"
#include <stdlib.h>

void init_event_source_impl(event_source* esrc);
void destroy_event_source_impl(event_source* esrc);
/* The specs are write-locked on entry */
void clear_event_source_impl_specs(event_source* esrc);
void remove_event_source_impl_spec(event_source* esrc, const UChar* const config_name);

SGLIB_DEFINE_RBTREE_FUNCTIONS(event_source_spec, left, right, color, EVENT_SOURCE_SPEC_COMPARATOR);

static void destroy_event_source_spec(event_source_spec* spec)
{
    udsfree(spec->name);
    yella_destroy_ptr_vector(spec->includes);
    yella_destroy_ptr_vector(spec->excludes);
    free(spec);
}

void add_or_replace_event_source_spec(event_source* esrc, event_source_spec* spec)
{
    event_source_spec* removed;

    yella_write_lock_reader_writer_lock(esrc->guard);
    if (sglib_event_source_spec_delete_if_member(&esrc->specs, spec, &removed) != 0)
        destroy_event_source_spec(removed);
    sglib_event_source_spec_add(&esrc->specs, spec);
    yella_unlock_reader_writer_lock(esrc->guard);
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
    clear_event_source_impl_specs(esrc);
    for (cur = sglib_event_source_spec_it_init(&itor, esrc->specs);
         cur != NULL;
         cur = sglib_event_source_spec_it_next(&itor))
    {
        destroy_event_source_spec(cur);
    }
    esrc->specs = NULL;
    yella_unlock_reader_writer_lock(esrc->guard);
}

void destroy_event_source(event_source* esrc)
{
    destroy_event_source_impl(esrc);
    clear_event_source_specs(esrc);
    yella_destroy_reader_writer_lock(esrc->guard);
    chucho_release_logger(esrc->lgr);
    free(esrc);
}

void remove_event_source_spec(event_source* esrc, const UChar* const name)
{
    event_source_spec to_remove;
    event_source_spec* removed;

    to_remove.name = (UChar*)name;
    yella_write_lock_reader_writer_lock(esrc->guard);
    remove_event_source_impl_spec(esrc, name);
    if (sglib_event_source_spec_delete_if_member(&esrc->specs, &to_remove, &removed))
        destroy_event_source_spec(removed);
    yella_unlock_reader_writer_lock(esrc->guard);
}
