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
#include <errno.h>

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

struct yella_reader_writer_lock
{
    pthread_rwlock_t rwlock;
};

struct yella_thread
{
    pthread_t thr;
};

struct yella_condition_variable
{
    pthread_cond_t cond;
};

void yella_broadcast_condition_variable(yella_condition_variable* cond)
{
    pthread_cond_broadcast(&cond->cond);
}

yella_condition_variable* yella_create_condition_variable(void)
{
    yella_condition_variable* result;

    result = malloc(sizeof(yella_condition_variable));
    pthread_cond_init(&result->cond, NULL);
    return result;
}

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

yella_reader_writer_lock* yella_create_reader_writer_lock(void)
{
    yella_reader_writer_lock* result;

    result = malloc(sizeof(yella_reader_writer_lock));
    pthread_rwlock_init(&result->rwlock, NULL);
    return result;
}

yella_thread* yella_create_thread(yella_thread_func f, void* arg)
{
    yella_thread* result;

    result = malloc(sizeof(yella_thread));
    pthread_create(&result->thr, NULL, (void*(*)(void*))f, arg);
    return result;
}

void yella_destroy_condition_variable(yella_condition_variable* cond)
{
    pthread_cond_destroy(&cond->cond);
    free(cond);
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

void yella_destroy_reader_writer_lock(yella_reader_writer_lock* lock)
{
    pthread_rwlock_destroy(&lock->rwlock);
    free(lock);
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

void yella_read_lock_reader_writer_lock(yella_reader_writer_lock* lock)
{
    pthread_rwlock_rdlock(&lock->rwlock);
}

void yella_signal_condition_variable(yella_condition_variable* cond)
{
    pthread_cond_signal(&cond->cond);
}

void yella_signal_event(yella_event* evt)
{
    pthread_mutex_lock(&evt->mtx);
    evt->signaled = true;
    pthread_cond_broadcast(&evt->cnd);
    pthread_mutex_unlock(&evt->mtx);
}

void yella_sleep_this_thread_milliseconds(size_t milliseconds)
{
    struct timespec to_wait;
    struct timespec remaining;
    int rc;

    to_wait.tv_sec = milliseconds / 1000;
    to_wait.tv_nsec = (milliseconds % 1000) * 1000000;
    do
    {
        rc = nanosleep(&to_wait, &remaining);
        to_wait = remaining;
    } while (rc != 0 && errno == EINTR);
}

void* yella_this_thread(void)
{
    return (void*)pthread_self();
}

void yella_unlock_mutex(yella_mutex* mtx)
{
    pthread_mutex_unlock(&mtx->mtx);
}

void yella_unlock_reader_writer_lock(yella_reader_writer_lock* lock)
{
    pthread_rwlock_unlock(&lock->rwlock);
}

bool yella_wait_milliseconds_for_condition_variable(yella_condition_variable* cond,
                                                    yella_mutex* mtx,
                                                    size_t milliseconds)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += milliseconds / 1000;
    ts.tv_nsec += (milliseconds % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000)
    {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec++;
    }
    return pthread_cond_timedwait(&cond->cond, &mtx->mtx, &ts) == 0;
}

void yella_wait_for_event(yella_event* evt)
{
    pthread_mutex_lock(&evt->mtx);
    while (!evt->signaled)
        pthread_cond_wait(&evt->cnd, &evt->mtx);
    pthread_mutex_unlock(&evt->mtx);
}

void yella_write_lock_reader_writer_lock(yella_reader_writer_lock* lock)
{
    pthread_rwlock_wrlock(&lock->rwlock);
}
