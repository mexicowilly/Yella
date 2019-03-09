#ifndef YELLA_STATE_DB_H__
#define YELLA_STATE_DB_H__

#include "export.h"
#include "plugin/file/element.h"
#include <stdbool.h>

typedef struct state_db state_db;

typedef enum
{
    STATE_DB_ACTION_KEEP,
    STATE_DB_ACTION_REMOVE
} state_db_removal_action;

state_db* create_state_db(const UChar* const config_name);
bool delete_from_state_db(state_db* st, const UChar* const elem_name);
void destroy_state_db(state_db* st, state_db_removal_action ra);
element* get_element_from_state_db(state_db* st, const UChar* const elem_name);
bool insert_into_state_db(state_db* st, const element* const elem);
bool update_into_state_db(state_db* st, const element* const elem);
const UChar* state_db_name(const state_db* const sdb);

#endif
