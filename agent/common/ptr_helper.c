#include <stdlib.h>

void yella_default_ptr_destructor(void* elem, void* udata)
{
    free(elem);
}

void* yella_default_ptr_copier(void* elem, void* udata)
{
    return NULL;
}

