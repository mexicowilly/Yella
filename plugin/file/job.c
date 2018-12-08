#include "plugin/file/job.h"
#include "plugin/file/file_name_matcher.h"
#include "common/uds_util.h"
#include <unicode/ustring.h>

static void run_one_include(const UChar* const incl, const job* const j)
{
    const UChar* special;

    special = first_unescaped_special_char(incl);
}

job* copy_job(const job* const j)
{
    job* result;

    result = malloc(sizeof(job));
    result->config_name = udsdup(j->config_name);
    result->includes = yella_copy_ptr_vector(j->includes);
    result->excludes = yella_copy_ptr_vector(j->excludes);
    result->agent_api = j->agent_api;
    return result;
}

job* create_job(const UChar* const cfg, const yella_agent_api* api)
{
    job* result;

    result = calloc(1, sizeof(job));
    result->config_name = udsnew(cfg);
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
    free(j->attr_types);
    free(j);
}

void run_job(const job* const j)
{
    int i;

    for (i = 0; i < yella_ptr_vector_size(j->includes); i++)
        run_one_include(yella_ptr_vector_at(j->includes, i), j);
}
