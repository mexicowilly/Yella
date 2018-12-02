#ifndef YELLA_PTR_DEQUE_H__
#define YELLA_PTR_DEQUE_H__

#include "export.h"
#include <stddef.h>

typedef struct yella_ptr_deque yella_ptr_deque;
typedef void (*yella_ptr_destructor)(void* elem, void* udata);
typedef void* (*yella_ptr_copier)(void* elem, void* udate);

YELLA_EXPORT yella_ptr_deque* yella_copy_ptr_deque(const yella_ptr_deque* const d);
YELLA_EXPORT yella_ptr_deque* yella_create_ptr_deque(void);
YELLA_EXPORT void yella_clear_ptr_deque(yella_ptr_deque* d);
YELLA_EXPORT void yella_destroy_ptr_deque(yella_ptr_deque* d);
YELLA_EXPORT void yella_pop_back_ptr_deque(yella_ptr_deque* d);
YELLA_EXPORT void yella_pop_front_ptr_deque(yella_ptr_deque* d);
YELLA_EXPORT size_t yella_ptr_deque_size(const yella_ptr_deque* const d);
YELLA_EXPORT void yella_push_back_ptr_deque(yella_ptr_deque* d, void* p);
YELLA_EXPORT void yella_push_front_ptr_deque(yella_ptr_deque* d, void* p);
YELLA_EXPORT void yella_set_ptr_deque_copier(yella_ptr_deque* d, yella_ptr_copier cp, void* udata);
YELLA_EXPORT void yella_set_ptr_deque_destructor(yella_ptr_deque* d, yella_ptr_destructor pd, void* udata);

#endif
