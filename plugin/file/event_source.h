#ifndef YELLA_EVENT_SOURCE_H__
#define YELLA_EVENT_SOURCE_H__

#include "common/uds_util.h"
#include "common/thread.h"
#include "common/sglib.h"
#include "common/uds.h"
#include <unicode/ustring.h>
#include <chucho/logger.h>

typedef struct event_source_spec
{
    uds name;
    /* These are both vectors of uds */
    yella_ptr_vector* includes;
    yella_ptr_vector* excludes;
    /* These are private */
    char color;
    struct event_source_spec* left;
    struct event_source_spec* right;
} event_source_spec;

typedef void (*event_source_callback)(const UChar* const config_name, const UChar* const fname, void* udata);

typedef struct event_source
{
    event_source_spec* specs;
    yella_reader_writer_lock* guard;
    event_source_callback callback;
    void* callback_udata;
    void* impl;
    chucho_logger_t* lgr;
} event_source;

/* These are private */
#define EVENT_SOURCE_SPEC_COMPARATOR(lhs, rhs) (u_strcmp(lhs->name, rhs->name))
SGLIB_DEFINE_RBTREE_PROTOTYPES(event_source_spec, left, right, color, EVENT_SOURCE_SPEC_COMPARATOR);
/* End private */

void add_or_replace_event_source_spec(event_source* esrc, event_source_spec* spec);
void clear_event_source_specs(event_source* esrc);
event_source* create_event_source(event_source_callback cb, void* cb_udata);
void destroy_event_source(event_source* esrc);
void remove_event_source_spec(event_source* esrc, const UChar* const name);

#endif