#include "plugin/file/state_db_pool.h"
#include "common/sglib.h"
#include "common/settings.h"
#include "common/time_util.h"
#include "common/text_util.h"
#include <unicode/ustring.h>
#include <chucho/log.h>

typedef struct state_db_node
{
    const UChar* name;
    state_db* db;
    uintmax_t time_last_used;
    char color;
    struct state_db_node* left;
    struct state_db_node* right;
} state_db_node;

struct state_db_pool
{
    state_db_node* nodes;
    size_t count;
};

#define STATE_DB_COMPARATOR(lhs, rhs) (u_strcmp(lhs->name, rhs->name))

SGLIB_DEFINE_RBTREE_PROTOTYPES(state_db_node, left, right, color, STATE_DB_COMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(state_db_node, left, right, color, STATE_DB_COMPARATOR);

state_db_pool* create_state_db_pool(void)
{
    state_db_pool* result;

    result = calloc(1, sizeof(state_db_pool));
    return result;
}

void destroy_state_db_pool(state_db_pool* pool)
{
    state_db_node* node;
    struct sglib_state_db_node_iterator itor;

    for (node = sglib_state_db_node_it_init(&itor, pool->nodes);
         node != NULL;
         node = sglib_state_db_node_it_next(&itor))
    {
        destroy_state_db(node->db);
        free(node);
    }
    free(pool);
}

state_db* get_state_db_from_pool(state_db_pool* pool, const UChar* const config_name)
{
    state_db_node* node;
    struct sglib_state_db_node_iterator itor;
    state_db_node* oldest;
    state_db_node* found;
    state_db_node to_find;
    size_t max_dbs;
    char* utf8;

    to_find.name = config_name;
    found = sglib_state_db_node_find_member(pool->nodes, &to_find);
    if (found == NULL)
    {
        max_dbs = *yella_settings_get_uint(u"file", u"max-spool-dbs");
        if (pool->count >= max_dbs)
        {
            oldest = NULL;
            for (node = sglib_state_db_node_it_init(&itor, pool->nodes);
                 node != NULL;
                 node = sglib_state_db_node_it_next(&itor))
            {
                if (oldest == NULL || node->time_last_used < oldest->time_last_used)
                    oldest = node;
            }
            if (oldest != NULL)
            {
                utf8 = yella_to_utf8(oldest->name);
                CHUCHO_C_INFO("yella.file.db", "Too many open state databases (%zu open, %zu maximum). Closing %s.", pool->count, max_dbs, utf8);
                free(utf8);
                sglib_state_db_node_delete(&pool->nodes, oldest);
                destroy_state_db(oldest->db);
                free(oldest);
            }
        }
        found = malloc(sizeof(state_db_node));
        found->db = create_state_db(config_name);
        if (found->db == NULL)
        {
            utf8 = yella_to_utf8(config_name);
            CHUCHO_C_FATAL("yella.file.db", "Unable to create state db for config %s", utf8);
            free(utf8);
            free(found);
            return NULL;
        }
        found->name = state_db_name(found->db);
        sglib_state_db_node_add(&pool->nodes, found);
        ++pool->count;
    }
    found->time_last_used = yella_millis_since_epoch();
    return found->db;
}

void remove_state_db_from_pool(state_db_pool* pool, const UChar* const config_name)
{
    state_db_node* removed;
    state_db_node to_find;

    to_find.name = config_name;
    if (sglib_state_db_node_delete_if_member(&pool->nodes, &to_find, &removed))
    {
        destroy_state_db(removed->db);
        free(removed);
    }
}
