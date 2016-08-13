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
#include <stdbool.h>

struct yella_event
{
    pthread_mutex_t mtx;
    pthread_cond_t cnd;
    bool signaled;
};

struct yella_mutex
{
    pthread_mutex_t mtx;
};

struct yella_thread
{
    pthread_t thr;
};

yella_event* yella_create_event(void)
{
    yella_event* result;

    result = malloc(sizeof(yella_event));
    pthread_mutex_init(&result->mtx, NULL);
    pthread_cond_init(&result->cnd, NULL);
    result->signaled = false;
    return result;
}

yella_mutex* yella_create_mutex(void)
{
    yella_mutex* result;

    result = malloc(sizeof(yella_mutex));
    pthread_mutex_init(&result->mtx, NULL);
    return result;
}

yella_thread* yella_create_thread(yella_thread_func f, void* arg)
{
    yella_thread* result;

    result = malloc(sizeof(yella_thread));
    pthread_create(&result->thr, NULL, (void*(*)(void*))f, arg);
    return result;
}

void yella_destroy_event(yella_event* evt)
{
    pthread_cond_destroy(&evt->cnd);
    pthread_mutex_destroy(&evt->mtx);
    free(evt);
}

void yella_destroy_mutex(yella_mutex* mtx)
{
    pthread_mutex_destroy(&mtx->mtx);
    free(mtx);
}

void yella_destroy_thread(yella_thread* thr)
{
    free(thr);
}

void yella_detach_thread(yella_thread* thr)
{
    pthread_detach(thr->thr);
}

void yella_join_thread(yella_thread* thr)
{
    pthread_join(thr->thr, NULL);
}

void yella_lock_mutex(yella_mutex* mtx)
{
    pthread_mutex_lock(&mtx->mtx);
}

void yella_signal_event(yella_event* evt)
{
    pthread_mutex_lock(&evt->mtx);
    evt->signaled = true;
    pthread_cond_broadcast(&evt->cnd);
    pthread_mutex_unlock(&evt->mtx);
}

void yella_unlock_mutex(yella_mutex* mtx)
{
    pthread_mutex_unlock(&mtx->mtx);
}

void yella_wait_for_event(yella_event* evt)
{
    pthread_mutex_lock(&evt->mtx);
    while (!evt->signaled)
        pthread_cond_wait(&evt->cnd, &evt->mtx);
    pthread_mutex_unlock(&evt->mtx);
}
