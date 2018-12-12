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
    /* These are both vectors of uds */
    yella_ptr_vector* includes;
    yella_ptr_vector* excludes;
    const yella_agent_api* agent_api;
    attribute_type* attr_types;
    size_t attr_type_count;
} job;

/* Ownership of state_db* is not transferred */
job* create_job(const UChar* const cfg, const yella_agent_api* api);
void destroy_job(job* j);
void run_job(const job* const j, state_db_pool* db_pool);

#endif
