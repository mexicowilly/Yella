#ifndef YELLA_JOB_QUEUE_H__
#define YELLA_JOB_QUEUE_H__

#include "plugin/file/job.h"
#include "plugin/file/state_db_pool.h"

typedef struct job_queue job_queue;

/* Ownership of the pool is not transferred */
job_queue* create_job_queue(state_db_pool* pool);
void destroy_job_queue(job_queue* jq);
void push_job_queue(job_queue* jq, job* jb);

#endif
