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

#if !defined(AGENT_H__)
#define AGENT_H__

#include <stdint.h>
#include <stddef.h>

typedef struct yella_agent yella_agent;

yella_agent* yella_create_agent(void);
void yella_destroy_agent(yella_agent* agent);
void yella_agent_run(yella_agent* agent);
void yella_agent_send(yella_agent* agent, const uint8_t* msg, size_t size);

#endif
