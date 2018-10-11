#include "common/sds_util.h"
#include "common/sds.h"

static void sds_dtor(void* elem, void* udata)
{
    sdsfree(elem);
}

void* sds_copier(void* elem, void* udate)
{
    return sdsdup(elem);
}

yella_ptr_vector* yella_create_sds_ptr_vector(void)
{
    yella_ptr_vector* v;

    v = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(v, sds_dtor, NULL);
    yella_set_ptr_vector_copier(v, sds_copier, NULL);
    return v;
}
