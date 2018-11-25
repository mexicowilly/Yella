#ifndef YELLA_JOB_H__
#define YELLA_JOB_H__

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
} job;

job* copy_job(const job* const j);
job* create_job(const uds cfg, const yella_agent_api* api);
void destroy_job(job* j);

#endif
