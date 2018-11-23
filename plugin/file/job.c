#include "plugin/file/job.h"
#include "common/uds_util.h"

job* create_job(const uds cfg, const yella_agent_api* api)
{
    job* result;

    result = malloc(sizeof(job));
    result->config_name = udsdup(cfg);
    result->includes = yella_create_uds_ptr_vector();
    result->excludes = yella_create_uds_ptr_vector();
    result->agent_api = api;
    return result;
}

void destroy_job(job* j)
{
    yella_destroy_ptr_vector(j->excludes);
    yella_destroy_ptr_vector(j->includes);
    udsfree(j->config_name);
    free(j);
}
