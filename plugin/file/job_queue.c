#include "plugin/file/job_queue.h"
#include "plugin/file/job.h"
#include "common/sglib.h"
#include "common/thread.h"
#include <unicode/ustring.h>

typedef struct queue
{
    job* jb;
    struct queue* previous;
    struct queue* next;
} queue;

struct job_queue
{
    queue* q;
    queue* last;
    size_t sz;
    yella_mutex* guard;
    yella_condition_variable* cond;
};

#define JOB_COMPARATOR(lhs, rhs) (u_strcmp(lhs->jb->config_name, rhs->jb->config_name))

SGLIB_DEFINE_DL_LIST_PROTOTYPES(queue, JOB_COMPARATOR, previous, next);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(queue, JOB_COMPARATOR, previous, next);

job_queue* create_job_queue(void)
{
    job_queue* result;

    result = calloc(1, sizeof(job_queue));
    result->guard = yella_create_mutex();
    result->cond = yella_create_condition_variable();
    return result;
}

void destroy_job_queue(job_queue* jq)
{
    struct sglib_queue_iterator itor;
    queue* q;

    yella_destroy_condition_variable(jq->cond);
    yella_destroy_mutex(jq->guard);
    for (q = sglib_queue_it_init(&itor, jq->q);
         q != NULL;
         q = sglib_queue_it_next(&itor))
    {
        destroy_job(q->jb);
        free(q);
    }
    free(jq);
}

void push_job_queue(job_queue* jq, job* jb)
{
    queue* q;

    q = malloc(sizeof(queue));
    q->jb = jb;
    yella_lock_mutex(jq->guard);
    sglib_queue_add_after(&jq->last, q);
    if (jq->last->next != NULL)
        jq->last = jq->last->next;
    if (jq->q == NULL)
        jq->q = jq->last;
    ++jq->sz;
    if (jq->sz == 1)
        yella_signal_condition_variable(jq->cond);
    yella_unlock_mutex(jq->guard);
}

void run_job_queue(job_queue* jq)
{

}
