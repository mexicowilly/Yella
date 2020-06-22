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

#include "common/ptr_vector.h"
#include <stdlib.h>
#include <string.h>

struct yella_ptr_vector
{
    void** data;
    size_t size;
    size_t capacity;
    yella_ptr_destructor destructor;
    void* destructor_udata;
    yella_ptr_copier copier;
    void* copier_udata;
};

yella_ptr_vector* yella_copy_ptr_vector(const yella_ptr_vector* const v)
{
    yella_ptr_vector* result;
    unsigned i;

    result = yella_create_ptr_vector();
    for (i = 0; i < yella_ptr_vector_size(v); i++)
        yella_push_back_ptr_vector(result, yella_ptr_vector_at_copy(v, i));
    result->destructor = v->destructor;
    result->destructor_udata = v->destructor_udata;
    result->copier = v->copier;
    result->copier_udata = v->copier_udata;
    return result;
}

yella_ptr_vector* yella_create_ptr_vector(void)
{
    yella_ptr_vector* result;

    result = malloc(sizeof(yella_ptr_vector));
    result->capacity = 20;
    result->size = 0;
    result->data = malloc(sizeof(void*) * result->capacity);
    result->destructor = yella_default_ptr_destructor;
    result->destructor_udata = NULL;
    result->copier = yella_default_ptr_copier;
    result->copier_udata = NULL;
    return result;
}

void yella_clear_ptr_vector(yella_ptr_vector* v)
{
    size_t i;

    if (v->destructor != NULL)
    {
        for (i = 0; i < v->size; i++)
            v->destructor(v->data[i], v->destructor_udata);
    }
    v->size = 0;
}

void yella_destroy_ptr_vector(yella_ptr_vector* v)
{
    if (v != NULL)
    {
        yella_clear_ptr_vector(v);
        free(v->data);
        free(v);
    }
}

void yella_erase_ptr_vector_at(yella_ptr_vector* v, unsigned off)
{
    size_t to_move;

    if (off < v->size)
    {
        if (v->destructor != NULL)
            v->destructor(v->data[off], v->destructor_udata);
        to_move = --v->size - off;
        if (to_move > 0)
            memmove(v->data + off, v->data + off + 1, to_move * sizeof(void*));
    }
}

void yella_pop_back_ptr_vector(yella_ptr_vector* v)
{
    if (v->size > 0)
        yella_erase_ptr_vector_at(v, v->size - 1);
}

void yella_pop_front_ptr_vector(yella_ptr_vector* v)
{
    if (v->size > 0)
        yella_erase_ptr_vector_at(v, 0);
}

void* yella_ptr_vector_at(const yella_ptr_vector* const v, unsigned off)
{
    return (off < v->size) ? v->data[off] : NULL;
}

void* yella_ptr_vector_at_copy(const yella_ptr_vector* const v, unsigned off)
{
    return v->copier(yella_ptr_vector_at(v, off), v->copier_udata);
}

void** yella_ptr_vector_data(const yella_ptr_vector* const v)
{
    return v->data;
}

size_t yella_ptr_vector_size(const yella_ptr_vector* const v)
{
    return v->size;
}

void yella_push_back_ptr_vector(yella_ptr_vector* v, void* p)
{
    if (v->size == v->capacity)
    {
        v->capacity = v->capacity * 1.5;
        v->data = realloc(v->data, sizeof(void*) * v->capacity);
    }
    v->data[v->size++] = p;
}

void yella_push_front_ptr_vector(yella_ptr_vector* v, void* p)
{
    if (v->size == v->capacity)
    {
        v->capacity = v->capacity * 1.5;
        v->data = realloc(v->data, sizeof(void*) * v->capacity);
    }
    memmove(v->data + 1, v->data, v->size * sizeof(void*));
    v->data[0] = p;
    v->size++;
}

void yella_set_ptr_vector_copier(yella_ptr_vector* v, yella_ptr_copier cp, void* udata)
{
    v->copier = cp;
    v->copier_udata = udata;
}

void yella_set_ptr_vector_destructor(yella_ptr_vector* v, yella_ptr_destructor pd, void* udata)
{
    v->destructor = pd;
    v->destructor_udata = udata;
}