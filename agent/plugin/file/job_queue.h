#ifndef YELLA_JOB_QUEUE_H__
#define YELLA_JOB_QUEUE_H__

#include "plugin/file/job.h"
#include "plugin/file/state_db_pool.h"
#include <chucho/logger.h>

typedef struct job_queue job_queue;

typedef struct job_queue_stats
{
    size_t max_size;
    size_t jobs_pushed;
    size_t jobs_run;
    uint64_t average_job_microseconds;
    uint64_t slowest_job_microseconds;
    uint64_t fastest_job_microseconds;
} job_queue_stats;

typedef void (*job_queue_empty_callback)(void* udata);

/* Ownership of the pool is not transferred */
YELLA_PRIV_EXPORT job_queue* create_job_queue(state_db_pool* pool);
YELLA_PRIV_EXPORT void destroy_job_queue(job_queue* jq);
YELLA_PRIV_EXPORT job_queue_stats get_job_queue_stats(job_queue* jq);
YELLA_PRIV_EXPORT void log_job_queue_stats(job_queue* jq, chucho_logger_t* lgr);
/* Returns the size of the queue after the push */
YELLA_PRIV_EXPORT size_t push_job_queue(job_queue* jq, job* jb);
/* As soon as the callback is called, it is removed.
 * The pattern is that once the queue fills, the
 * event source is paused until the queue is empty.
 * Automatic removal of the callback in this scenario
 * simplifies usage. */
YELLA_PRIV_EXPORT void set_job_queue_empty_callback(job_queue* jq, job_queue_empty_callback cb, void* udata);

#endif
