#ifndef YELLA_STATE_DB_POOL_H__
#define YELLA_STATE_DB_POOL_H__

#include "plugin/file/state_db.h"

typedef struct state_db_pool state_db_pool;

YELLA_PRIV_EXPORT state_db_pool* create_state_db_pool(void);
YELLA_PRIV_EXPORT void destroy_state_db_pool(state_db_pool* pool);
YELLA_PRIV_EXPORT state_db* get_state_db_from_pool(state_db_pool* pool, const UChar* const config_name);
YELLA_PRIV_EXPORT void remove_state_db_from_pool(state_db_pool* pool, const UChar* const config_name);
YELLA_PRIV_EXPORT size_t state_db_pool_size(const state_db_pool* const pool);

#endif
