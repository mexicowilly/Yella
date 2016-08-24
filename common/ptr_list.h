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

#if !defined(PTR_LIST_H__)
#define PTR_LIST_H__

#include "export.h"
#include <stddef.h>

typedef struct yella_ptr_list yella_ptr_list;
typedef struct yella_ptr_list_iterator yella_ptr_list_iterator;

YELLA_EXPORT yella_ptr_list* yella_create_ptr_list(void* first);
YELLA_EXPORT void yella_destroy_ptr_list(yella_ptr_list* l);
YELLA_EXPORT size_t yella_ptr_list_size(yella_ptr_list* l);
YELLA_EXPORT void yella_push_back_ptr_list(yella_ptr_list* l, void* p);
YELLA_EXPORT void yella_push_front_ptr_list(yella_ptr_list* l, void* p);
YELLA_EXPORT void yella_reverse_ptr_list(yella_ptr_list* l);

YELLA_EXPORT yella_ptr_list_iterator* yella_create_ptr_list_iterator(yella_ptr_list* l);
YELLA_EXPORT void yella_destroy_ptr_list_iterator(yella_ptr_list_iterator* i);
YELLA_EXPORT void* yella_ptr_list_iterator_next(yella_ptr_list_iterator* i);

#endif
