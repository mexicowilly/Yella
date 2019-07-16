#ifndef YELLA_JOB_H__
#define YELLA_JOB_H__

#include "plugin/file/attribute.h"
#include "plugin/file/state_db_pool.h"
#include "plugin/file/accumulator.h"
#include "common/ptr_vector.h"
#include "common/uds.h"
#include "plugin/plugin.h"

typedef struct job
{
    uds config_name;
    uds recipient;
    accumulator* acc;
    /* These are both vectors of uds */
    yella_ptr_vector* includes;
    yella_ptr_vector* excludes;
    attribute_type* attr_types;
    size_t attr_type_count;
} job;

YELLA_PRIV_EXPORT job* create_job(const UChar* const cfg_name,
                                  const UChar* const recipient,
                                  accumulator* acc);
YELLA_PRIV_EXPORT void destroy_job(job* j);
/* Ownership of db_pool is not transferred */
YELLA_PRIV_EXPORT void run_job(const job* const j, state_db_pool* db_pool);

#endif
