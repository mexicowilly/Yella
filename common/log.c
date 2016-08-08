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

#include "log.h"
#include "text_util.h"
#include "sglib.h"
#include <string.h>
#include <stdlib.h>

typedef struct logger
{
    char* name;
    chucho_logger* lgr;
    char color;
    struct logger* left;
    struct logger* right;
} logger;

#define YELLA_LOGGERS_COMPARE(x, y) strcmp((x->name), (y->name))

SGLIB_DEFINE_RBTREE_PROTOTYPES(logger, left, right, color, YELLA_LOGGERS_COMPARE);

SGLIB_DEFINE_RBTREE_FUNCTIONS(logger, left, right, color, YELLA_LOGGERS_COMPARE);

static logger* lgrs = NULL;

void yella_destroy_loggers(void)
{
    logger* elem;
    struct sglib_logger_iterator itor;

    for (elem = sglib_logger_it_init(&itor, lgrs);
         elem != NULL;
         elem = sglib_logger_it_next(&itor))
    {
        free(elem->name);
        chucho_release_logger(elem->lgr);
        free(elem);
    }
}

chucho_logger* yella_logger(const char* const name)
{
    logger to_find;
    logger* found;

    to_find.name = (char*)name;
    found = sglib_logger_find_member(lgrs, &to_find);
    if (found == NULL)
    {
        found = malloc(sizeof(logger));
        found->name = text_dup(name);
        chucho_create_logger(&found->lgr, found->name);
        sglib_logger_add(&lgrs, found);
    }
    return found->lgr;
}
