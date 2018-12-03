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
    yella_mutex* guard;
    yella_condition_variable* cond;
};

#define JOB_COMPARATOR(lhs, rhs) (u_strcmp(lhs->jb->config_name, rhs->jb->config_name))

SGLIB_DEFINE_DL_LIST_PROTOTYPES(queue, JOB_COMPARATOR, previous, next);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(queue, JOB_COMPARATOR, previous, next);

job_queue* create_job_queue(void)
{
    job_queue* result;
}
