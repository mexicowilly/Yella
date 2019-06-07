#ifndef YELLA_JOB_QUEUE_H__
#define YELLA_JOB_QUEUE_H__

#include "plugin/file/job.h"
#include "plugin/file/state_db_pool.h"

typedef struct job_queue job_queue;

typedef void (*job_queue_empty_callback)(void* udata);

/* Ownership of the pool is not transferred */
job_queue* create_job_queue(state_db_pool* pool);
void destroy_job_queue(job_queue* jq);
/* Returns the size of the queue after the push */
size_t push_job_queue(job_queue* jq, job* jb);
/* As soon as the callback is called, it is removed.
 * The pattern is that once, the queue fills, the
 * event source is paused until the queue is empty.
 * Automatic removal if the callback in this scenario
 * simplifies usage. */
void set_job_queue_empty_callback(job_queue* jq, job_queue_empty_callback cb, void* udata);

#endif
