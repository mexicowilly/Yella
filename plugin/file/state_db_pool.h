#ifndef YELLA_STATE_DB_POOL_H__
#define YELLA_STATE_DB_POOL_H__

#include "plugin/file/state_db.h"

typedef struct state_db_pool state_db_pool;

state_db_pool* create_state_db_pool(void);
void destroy_state_db_pool(state_db_pool* pool);
state_db* get_state_db_from_pool(state_db_pool* pool, const UChar* const config_name);
void remove_state_db_from_pool(state_db_pool* pool, const UChar* const config_name);

#endif
