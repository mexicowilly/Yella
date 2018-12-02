#include <stdlib.h>

#define NODE_SIZE (1024 / sizeof(void*))
#define INITIAL_MAP_SIZE 8

typedef struct ptr_deque_itor
{
    void* current;
    void* first;
    void* last;
    void** node;
} ptr_deque_itor;

struct yella_ptr_deque
{
    ptr_deque_itor start;
    ptr_deque_itor finish;
    void** map;
    size_t map_size;
};

static void set_itor_node(ptr_deque_itor* itor, void** node)
{
    itor->node = node;
    itor->first = *itor->node;
    itor->last = itor->first + NODE_SIZE;
}

yella_ptr_deque* yella_create_ptr_deque(void)
{
    yella_ptr_deque* result;
    void** nstart;
    void** nfinish;
    void** cur;

    result = calloc(1, sizeof(yella_ptr_deque));
    result->map_size = INITIAL_MAP_SIZE;
    result->map = malloc(result->map_size * sizeof(void*));
    nstart = result->map + (result->map_size / 2);
    nfinish = nstart;
    set_itor_node(&result->start, result->map + (result->map_size / 2));
    set_itor_node(&result->finish, result->start.node - 1);
    result->start.current = result->start.first;
    result->finish.current = result->finish.first;
}
