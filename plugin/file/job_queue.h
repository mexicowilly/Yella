#ifndef YELLA_JOB_QUEUE_H__
#define YELLA_JOB_QUEUE_H__

#include "plugin/file/job.h"

typedef struct job_queue job_queue;

job_queue* create_job_queue(void);
void destroy_job_queue(job_queue* jq);
void push_job_queue(job_queue* jq, job* jb);
void run_job_queue(job_queue* jq);

#endif
