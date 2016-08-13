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

#if !defined(THREAD_H__)
#define THREAD_H__

#include "export.h"

/**
 * Events
 */
typedef struct yella_event yella_event;

YELLA_EXPORT yella_event* yella_create_event(void);
YELLA_EXPORT void yella_destroy_event(yella_event* evt);
YELLA_EXPORT void yella_signal_event(yella_event* evt);
YELLA_EXPORT void yella_wait_for_event(yella_event* evt);

/**
 * Mutexes
 */
typedef struct yella_mutex yella_mutex;

YELLA_EXPORT yella_mutex* yella_create_mutex(void);
YELLA_EXPORT void yella_destroy_mutex(yella_mutex* mtx);
YELLA_EXPORT void yella_lock_mutex(yella_mutex* mtx);
YELLA_EXPORT void yella_unlock_mutex(yella_mutex* mtx);

/**
 * Threads
 */
typedef struct yella_thread yella_thread;
typedef void (*yella_thread_func)(void*);

YELLA_EXPORT yella_thread* yella_create_thread(yella_thread_func f, void* arg);
YELLA_EXPORT void yella_destroy_thread(yella_thread* thr);
YELLA_EXPORT void yella_detach_thread(yella_thread* thr);
YELLA_EXPORT void yella_join_thread(yella_thread* thr);

#endif
