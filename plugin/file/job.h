#ifndef YELLA_JOB_H__
#define YELLA_JOB_H__

#include "plugin/file/attribute.h"
#include "plugin/file/state_db_pool.h"
#include "common/ptr_vector.h"
#include "common/uds.h"
#include "plugin/plugin.h"

typedef struct job
{
    uds config_name;
    uds topic;
    void* agent;
    /* These are both vectors of uds */
    yella_ptr_vector* includes;
    yella_ptr_vector* excludes;
    const yella_agent_api* agent_api;
    attribute_type* attr_types;
    size_t attr_type_count;
} job;

job* create_job(const UChar* const cfg_name,
                const yella_agent_api* api,
                const UChar* const topic,
                void* agnt);
void destroy_job(job* j);
/* Ownership of db_pool is not transferred */
void run_job(const job* const j, state_db_pool* db_pool);

#endif
