#include "plugin/file/job_queue.h"
#include "plugin/file/job.h"
#include "common/sglib.h"
#include "common/thread.h"
#include "common/time_util.h"
#include <unicode/ustring.h>
#include <chucho/log.h>
#include <cjson/cJSON.h>

typedef struct queue
{
    job* jb;
    struct queue* previous;
    struct queue* next;
} queue;

struct job_queue
{
    queue* q;
    size_t sz;
    yella_mutex* guard;
    yella_condition_variable* cond;
    yella_thread* runner;
    bool should_stop;
    state_db_pool* db_pool;
    chucho_logger_t* lgr;
    job_queue_empty_callback cb;
    void* cb_data;
    job_queue_stats stats;
    size_t accumulated_size;
    uint64_t accumulated_microseconds;
};

#define JOB_COMPARATOR(lhs, rhs) (u_strcmp(lhs->jb->config_name, rhs->jb->config_name))

SGLIB_DEFINE_DL_LIST_PROTOTYPES(queue, JOB_COMPARATOR, previous, next);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(queue, JOB_COMPARATOR, previous, next);

static void job_queue_main(void* udata)
{
    job_queue* jq;
    queue* front;
    uint64_t start_micros;
    uint64_t job_micros;

    jq = (job_queue*)udata;
    CHUCHO_C_INFO(jq->lgr, "Job queue thread starting");
    while (true)
    {
        yella_lock_mutex(jq->guard);
        while (!jq->should_stop && jq->sz == 0 && jq->cb == NULL)
           yella_wait_milliseconds_for_condition_variable(jq->cond, jq->guard, 250);
        if (jq->should_stop)
        {
            yella_unlock_mutex(jq->guard);
            break;
        }
        if (jq->sz == 0)
        {
            if (jq->cb != NULL)
            {
                jq->cb(jq->cb_data);
                jq->cb = NULL;
                jq->cb_data = NULL;
            }
            yella_unlock_mutex(jq->guard);
        }
        else
        {
            front = sglib_queue_get_first(jq->q);
            sglib_queue_delete(&jq->q, front);
            --jq->sz;
            --jq->accumulated_size;
            yella_unlock_mutex(jq->guard);
            start_micros = yella_microseconds_since_epoch();
            run_job(front->jb, jq->db_pool);
            job_micros = yella_microseconds_since_epoch() - start_micros;
            jq->accumulated_microseconds += job_micros;
            destroy_job(front->jb);
            free(front);
            yella_lock_mutex(jq->guard);
            if (++jq->stats.jobs_run == 1)
            {
                jq->stats.fastest_job_microseconds = job_micros;
                jq->stats.slowest_job_microseconds = job_micros;
            }
            else
            {
                if (job_micros > jq->stats.fastest_job_microseconds)
                    jq->stats.fastest_job_microseconds = job_micros;
                if (job_micros < jq->stats.slowest_job_microseconds)
                    jq->stats.slowest_job_microseconds = job_micros;
            }
            yella_unlock_mutex(jq->guard);
        }
    }
    CHUCHO_C_INFO(jq->lgr, "Job queue thread ending");
}

job_queue* create_job_queue(state_db_pool* pool)
{
    job_queue* result;

    result = calloc(1, sizeof(job_queue));
    result->lgr = chucho_get_logger("yella.file.job-queue");
    result->guard = yella_create_mutex();
    result->cond = yella_create_condition_variable();
    result->runner = yella_create_thread(job_queue_main, result);
    result->db_pool = pool;
    return result;
}

void destroy_job_queue(job_queue* jq)
{
    struct sglib_queue_iterator itor;
    queue* q;

    yella_lock_mutex(jq->guard);
    jq->should_stop = true;
    yella_signal_condition_variable(jq->cond);
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
    chucho_release_logger(jq->lgr);
    free(jq);
}

job_queue_stats get_job_queue_stats(job_queue* jq)
{
    job_queue_stats result;

    yella_lock_mutex(jq->guard);
    result = jq->stats;
    result.average_size = jq->accumulated_size == 0 ? 0 : jq->accumulated_size / (result.jobs_pushed + result.jobs_run);
    result.average_job_microseconds = jq->accumulated_microseconds == 0 ? 0 : jq->accumulated_microseconds / result.jobs_run;
    yella_unlock_mutex(jq->guard);
    return result;
}

void log_job_queue_stats(job_queue* jq, chucho_logger_t* lgr)
{
    cJSON* json;
    job_queue_stats stats;
    char* log_msg;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        stats = get_job_queue_stats(jq);
        json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "average_size", stats.average_size);
        cJSON_AddNumberToObject(json, "max_size", stats.max_size);
        cJSON_AddNumberToObject(json, "jobs_pushed", stats.jobs_pushed);
        cJSON_AddNumberToObject(json, "average_job_microseconds", stats.average_job_microseconds);
        cJSON_AddNumberToObject(json, "slowest_job_microseconds", stats.slowest_job_microseconds);
        cJSON_AddNumberToObject(json, "fastest_job_microseconds", stats.fastest_job_microseconds);
        cJSON_AddNumberToObject(json, "jobs_run", stats.jobs_run);
        log_msg = cJSON_PrintUnformatted(json);
        cJSON_Delete(json);
        CHUCHO_C_INFO(lgr, "Job queue stats: %s", log_msg);
        free(log_msg);
    }
}

size_t push_job_queue(job_queue* jq, job* jb)
{
    queue* q;
    size_t cur_sz;

    q = calloc(1, sizeof(queue));
    q->jb = jb;
    yella_lock_mutex(jq->guard);
    sglib_queue_concat(&jq->q, q);
    cur_sz = ++jq->sz;
    if (cur_sz == 1)
        yella_signal_condition_variable(jq->cond);
    ++jq->stats.jobs_pushed;
    ++jq->accumulated_size;
    if (cur_sz > jq->stats.max_size)
        jq->stats.max_size = cur_sz;
    yella_unlock_mutex(jq->guard);
    return cur_sz;
}

void set_job_queue_empty_callback(job_queue* jq, job_queue_empty_callback cb, void* udata)
{
    yella_lock_mutex(jq->guard);
    jq->cb = cb;
    jq->cb_data = udata;
    yella_signal_condition_variable(jq->cond);
    yella_unlock_mutex(jq->guard);
}
