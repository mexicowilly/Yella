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

#include "common/thread.h"
#include <pthread.h>
#include <stdlib.h>

struct yella_mutex
{
    pthread_mutex_t mtx;
};

yella_mutex* yella_create_mutex(void)
{
    yella_mutex* result;

    result = malloc(sizeof(yella_mutex));
    pthread_mutex_init(&result->mtx, NULL);
    return result;
}

void yella_destroy_mutex(yella_mutex* mtx)
{
    pthread_mutex_destroy(&mtx->mtx);
    free(mtx);
}

void yella_lock_mutex(yella_mutex* mtx)
{
    pthread_mutex_lock(&mtx->mtx);
}

void yella_unlock_mutex(yella_mutex* mtx)
{
    pthread_mutex_unlock(&mtx->mtx);
}
