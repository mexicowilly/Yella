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

typedef struct yella_mutex yella_mutex;

YELLA_EXPORT yella_mutex* yella_create_mutex(void);
YELLA_EXPORT void yella_destroy_mutex(yella_mutex* mtx);
YELLA_EXPORT void yella_lock_mutex(yella_mutex* mtx);
YELLA_EXPORT void yella_unlock_mutex(yella_mutex* mtx);

#endif
