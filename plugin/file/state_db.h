#ifndef YELLA_STATE_DB_H__
#define YELLA_STATE_DB_H__

#include "plugin/file/element.h"
#include <unicode/utypes.h>
#include <stdbool.h>

typedef struct state_db state_db;

state_db* create_state_db(const UChar* const config_name);
bool delete_from_state_db(state_db* st, const UChar* const elem_name);
void destroy_state_db(state_db* st);
uint8_t* get_attributes_from_state_db(state_db* st, const UChar* const elem_name);
bool insert_into_state_db(state_db* st, const element* const elem);
bool update_into_state_db(state_db* st, const element* const elem);

#endif
