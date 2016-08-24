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

#include "common/ptr_list.h"
#include <stdlib.h>
#include "sglib.h"

#define YELLA_PTR_LIST_COMPARATOR(x, y) (x->ptr) < (y->ptr)

struct yella_ptr_list
{
    void* ptr;
    yella_ptr_list* previous;
    yella_ptr_list* next;
};

SGLIB_DEFINE_DL_LIST_PROTOTYPES(yella_ptr_list,
                                YELLA_PTR_LIST_COMPARATOR,
                                previous,
                                next);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(yella_ptr_list,
                               YELLA_PTR_LIST_COMPARATOR,
                               previous,
                               next);

struct yella_ptr_list_iterator
{
    struct sglib_yella_ptr_list_iterator itor;
    yella_ptr_list* list;
};

yella_ptr_list* yella_create_ptr_list(void* first)
{
    yella_ptr_list* l;
    yella_ptr_list* elem;

    l = NULL;
    elem = malloc(sizeof(yella_ptr_list));
    elem->ptr = first;
    sglib_yella_ptr_list_add(&l, elem);
    return l;
}

void yella_destroy_ptr_list(yella_ptr_list* l)
{
    struct sglib_yella_ptr_list_iterator itor;
    yella_ptr_list* elem;

    for (elem = sglib_yella_ptr_list_it_init(&itor, l);
         elem != NULL;
         elem = sglib_yella_ptr_list_it_next(&itor))
    {
        free(elem->ptr);
        free(elem);
    }
}

size_t yella_ptr_list_size(yella_ptr_list* l)
{
    return sglib_yella_ptr_list_len(l);
}

void yella_push_back_ptr_list(yella_ptr_list* l, void* p)
{
    yella_ptr_list* elem;
    yella_ptr_list* last;

    elem = malloc(sizeof(yella_ptr_list));
    elem->ptr = p;
    last = sglib_yella_ptr_list_get_last(l);
    sglib_yella_ptr_list_add_after(&last, elem);
}

void yella_push_front_ptr_list(yella_ptr_list* l, void* p)
{
    yella_ptr_list* elem;
    yella_ptr_list* first;

    elem = malloc(sizeof(yella_ptr_list));
    elem->ptr = p;
    first = sglib_yella_ptr_list_get_first(l);
    sglib_yella_ptr_list_add_before(&first, elem);
}

void yella_reverse_ptr_list(yella_ptr_list* l)
{
    sglib_yella_ptr_list_reverse(&l);
}

yella_ptr_list_iterator* yella_create_ptr_list_iterator(yella_ptr_list* l)
{
    yella_ptr_list_iterator* result;
    yella_ptr_list* first;

    first = sglib_yella_ptr_list_get_first(l);
    result = malloc(sizeof(yella_ptr_list_iterator));
    result->list = first;
    return result;
}

void yella_destroy_ptr_list_iterator(yella_ptr_list_iterator* i)
{
    free(i);
}

void* yella_ptr_list_iterator_next(yella_ptr_list_iterator* i)
{
    yella_ptr_list* node;

    if (i->list == NULL)
    {
        node = sglib_yella_ptr_list_it_next(&i->itor);
    }
    else
    {
        node = sglib_yella_ptr_list_it_init(&i->itor, i->list);
        i->list = NULL;
    }
    return (node == NULL) ? NULL : node->ptr;
}

