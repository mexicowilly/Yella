/*
 * Copyright 2016 Will Mason
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if !defined(PTR_VECTOR_H__)
#define PTR_VECTOR_H__

#include "export.h"
#include "common/ptr_helper.h"
#include <stddef.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct yella_ptr_vector yella_ptr_vector;

YELLA_EXPORT void yella_assign_ptr_vector(yella_ptr_vector* v, const yella_ptr_vector* const other);
YELLA_EXPORT yella_ptr_vector* yella_copy_ptr_vector(const yella_ptr_vector* const v);
YELLA_EXPORT yella_ptr_vector* yella_create_ptr_vector(void);
YELLA_EXPORT void yella_clear_ptr_vector(yella_ptr_vector* v);
YELLA_EXPORT void yella_destroy_ptr_vector(yella_ptr_vector* v);
YELLA_EXPORT void yella_erase_ptr_vector_at(yella_ptr_vector* v, unsigned off);
#if !defined(NDEBUG)
YELLA_EXPORT void yella_finalize_ptr_vectors();
#endif
YELLA_EXPORT void yella_pop_back_ptr_vector(yella_ptr_vector* v);
YELLA_EXPORT void yella_pop_front_ptr_vector(yella_ptr_vector* v);
YELLA_EXPORT void* yella_ptr_vector_at(const yella_ptr_vector* const v, unsigned off);
YELLA_EXPORT void* yella_ptr_vector_at_copy(const yella_ptr_vector* const v, unsigned off);
YELLA_EXPORT void** yella_ptr_vector_data(const yella_ptr_vector* const v);
YELLA_EXPORT int yella_ptr_vector_id(const yella_ptr_vector* const v);
YELLA_EXPORT size_t yella_ptr_vector_size(const yella_ptr_vector* const v);
YELLA_EXPORT void yella_push_back_ptr_vector(yella_ptr_vector* v, void* p);
YELLA_EXPORT void yella_push_front_ptr_vector(yella_ptr_vector* v, void* p);
YELLA_EXPORT void yella_set_ptr_vector_copier(yella_ptr_vector* v, yella_ptr_copier cp, void* udata);
YELLA_EXPORT void yella_set_ptr_vector_destructor(yella_ptr_vector* v, yella_ptr_destructor pd, void* udata);

#if defined(__cplusplus)
}
#endif

#endif
