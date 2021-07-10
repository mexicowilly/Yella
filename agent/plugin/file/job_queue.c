#include "plugin/file/job_queue.h"
#include "plugin/file/job.h"
#include "common/sglib.h"
#include "common/thread.h"
#include "common/time_util.h"
#include "common/text_util.h"
#include "common/yaml_util.h"
#include <unicode/ustring.h>
#include <chucho/log.h>
#include <inttypes.h>

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
    uint64_t accumulated_microseconds;
    chucho_logger_t* job_lgr;
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
    char* utf8;

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
            yella_unlock_mutex(jq->guard);
            if (chucho_logger_permits(jq->lgr, CHUCHO_INFO))
            {
                utf8 = yella_to_utf8(front->jb->config_name);
                CHUCHO_C_INFO(jq->lgr, "Starting job for config '%s'", utf8);
                free(utf8);
            }
            start_micros = yella_microseconds_since_epoch();
            run_job(front->jb, jq->db_pool, jq->job_lgr);
            job_micros = yella_microseconds_since_epoch() - start_micros;
            if (chucho_logger_permits(jq->lgr, CHUCHO_INFO))
            {
                utf8 = yella_to_utf8(front->jb->config_name);
                CHUCHO_C_INFO(jq->lgr, "Ended job for config '%s' (%" PRId64 " us)", utf8, job_micros);
                free(utf8);
            }
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
                if (job_micros < jq->stats.fastest_job_microseconds)
                    jq->stats.fastest_job_microseconds = job_micros;
                if (job_micros > jq->stats.slowest_job_microseconds)
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
    result->lgr = chucho_get_logger("file.job-queue");
    result->job_lgr = chucho_get_logger("file.job");
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

    log_job_queue_stats(jq, jq->lgr);
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
    chucho_release_logger(jq->job_lgr);
    chucho_release_logger(jq->lgr);
    free(jq);
}

job_queue_stats get_job_queue_stats(job_queue* jq)
{
    job_queue_stats result;

    yella_lock_mutex(jq->guard);
    result = jq->stats;
    result.average_job_microseconds = jq->accumulated_microseconds == 0 ? 0 : jq->accumulated_microseconds / result.jobs_run;
    yella_unlock_mutex(jq->guard);
    return result;
}

void log_job_queue_stats(job_queue* jq, chucho_logger_t* lgr)
{
    job_queue_stats stats;
    yaml_document_t doc;
    char* utf8;
    int top;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        stats = get_job_queue_stats(jq);
        yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
        top = yaml_document_add_mapping(&doc, NULL, YAML_FLOW_MAPPING_STYLE);
        yella_add_yaml_number_mapping(&doc, top, "max_size", stats.max_size);
        yella_add_yaml_number_mapping(&doc, top, "jobs_pushed", stats.jobs_pushed);
        yella_add_yaml_number_mapping(&doc, top, "jobs_run", stats.jobs_run);
        yella_add_yaml_number_mapping(&doc, top, "average_job_us", stats.average_job_microseconds);
        yella_add_yaml_number_mapping(&doc, top, "slowest_job_us", stats.slowest_job_microseconds);
        yella_add_yaml_number_mapping(&doc, top, "fastest_job_us", stats.fastest_job_microseconds);
        utf8 = yella_emit_yaml(&doc);
        yaml_document_delete(&doc);
        CHUCHO_C_INFO(lgr, "Job queue stats: %s", utf8);
        free(utf8);
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
