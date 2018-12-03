#ifndef YELLA_PTR_HELPER_H__
#define YELLA_PTR_HELPER_H__

typedef void (*yella_ptr_destructor)(void* elem, void* udata);
typedef void* (*yella_ptr_copier)(void* elem, void* udate);

void yella_default_ptr_destructor(void* elem, void* udata);
void* yella_default_ptr_copier(void* elem, void* udata);

#endif
