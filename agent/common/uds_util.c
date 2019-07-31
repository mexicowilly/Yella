#include "common/uds_util.h"
#include "common/uds.h"

static void uds_dtor(void* elem, void* udata)
{
    udsfree(elem);
}

void* uds_copier(void* elem, void* udate)
{
    return udsdup(elem);
}

yella_ptr_vector* yella_create_uds_ptr_vector(void)
{
    yella_ptr_vector* v;

    v = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(v, uds_dtor, NULL);
    yella_set_ptr_vector_copier(v, uds_copier, NULL);
    return v;
}
