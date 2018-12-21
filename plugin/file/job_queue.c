#include "plugin/file/job_queue.h"
#include "plugin/file/job.h"
#include "common/sglib.h"
#include "common/thread.h"
#include "state_db_pool.h"
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
    yella_thread* runner;
    bool should_stop;
};

#define JOB_COMPARATOR(lhs, rhs) (u_strcmp(lhs->jb->config_name, rhs->jb->config_name))

SGLIB_DEFINE_DL_LIST_PROTOTYPES(queue, JOB_COMPARATOR, previous, next);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(queue, JOB_COMPARATOR, previous, next);

static void job_queue_main(void* udata)
{
    job_queue* jq;
    queue* front;
    state_db_pool* db_pool;

    jq = (job_queue*)udata;
    db_pool = create_state_db_pool();
    while (true)
    {
        yella_lock_mutex(jq->guard);
        while (!jq->should_stop && jq->sz == 0)
            yella_wait_milliseconds_for_condition_variable(jq->cond, jq->guard, 250);
        if (jq->should_stop)
        {
            yella_unlock_mutex(jq->guard);
            break;
        }
        if (jq->sz == 0)
        {
            yella_unlock_mutex(jq->guard);
        }
        else
        {
            front = sglib_queue_get_first(jq->q);
            sglib_queue_delete(&jq->q, front);
            --jq->sz;
            yella_unlock_mutex(jq->guard);
            run_job(front->jb, db_pool);
            destroy_job(front->jb);
            free(front);
        }
    }
    destroy_state_db_pool(db_pool);
}

job_queue* create_job_queue(void)
{
    job_queue* result;

    result = calloc(1, sizeof(job_queue));
    result->guard = yella_create_mutex();
    result->cond = yella_create_condition_variable();
    result->runner = yella_create_thread(job_queue_main, result);
    return result;
}

void destroy_job_queue(job_queue* jq)
{
    struct sglib_queue_iterator itor;
    queue* q;

    yella_lock_mutex(jq->guard);
    jq->should_stop = true;
    yella_unlock_mutex(jq->guard);
    yella_join_thread(jq->runner);
    yella_destroy_thread(jq->runner);
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
