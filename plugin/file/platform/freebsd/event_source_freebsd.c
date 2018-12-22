#include "plugin/file/event_source.h"
#include "plugin/file/file_name_matcher.h"
#include "common/process.h"
#include <chucho/log.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <libprocstat.h>

typedef struct name_node
{
    pid_t pid;
    int fd;
    uds name;
    char color;
    struct name_node* left;
    struct name_node* right;
} name_node;

typedef struct event_source_freebsd
{
    yella_process* dtrace;
    yella_thread* worker;
    atomic_bool should_stop;
    struct procstat* pstat;
    name_node* names;
} event_source_freebsd;

static int name_node_comparator(name_node* lhs, name_node* rhs)
{
    int result;

    result = lhs->pid - rhs->pid;
    if (result == 0)
        result = lhs->fd - rhs->fd;
    return result;
}

SGLIB_DEFINE_RBTREE_PROTOTYPES(name_node, left, right, color, name_node_comparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(name_node, left, right, color, name_node_comparator);

static char* cwd_of_pid(pid_t pid, struct procstat* pstat, chucho_logger_t* lgr)
{
    struct kinfo_proc* kp;
    struct filestat_list* files;
    struct filestat* fst;
    char* result;
    unsigned cnt;

    // TODO: error handling
    kp = procstat_getprocs(pstat, KERN_PROC_PID, pid, &cnt);
    files = procstat_getfiles(pstat, kp, 0);
    STAILQ_FOREACH(fst, files, next)
    {
        if ((fst->fs_uflags & PS_FST_UFLAG_CDIR) != 0)
        {
            result = strdup(fst->fs_path);
            break;
        }
    }
    procstat_freefiles(pstat, files);
    procstat_freeprocs(pstat, kp);
    return result;
}

static bool file_name_matches_any(const event_source* const esrc, const UChar* const fname)
{
    int i;
    struct sglib_event_source_spec_iterator spec_itor;
    event_source_spec* cur_spec;

    for (cur_spec = sglib_event_source_spec_it_init(&spec_itor, esrc->specs);
         cur_spec != NULL;
         cur_spec = sglib_event_source_spec_it_next(&spec_itor))
    {
        for (i = 0; i < yella_ptr_vector_size(cur_spec->excludes); i++)
        {
            if (file_name_matches(fname, yella_ptr_vector_at(cur_spec->excludes, i)))
                return false;
        }
        for (i = 0; i < yella_ptr_vector_size(cur_spec->includes); i++)
        {
            if (file_name_matches(fname, yella_ptr_vector_at(cur_spec->includes, i)))
                return true;
        }
    }
    return false;
}

static void worker_main(void* udata)
{
    event_source* esrc;
    event_source_freebsd* esf;
    FILE* reader;
    struct pollfd pfd;
    int rc;

    esrc = udata;
    esf = esrc->impl;
    reader = yella_process_get_reader(esf->dtrace);
    pfd.fd = fileno(reader);
    pfd.events = POLLIN;
    while (!esf->should_stop)
    {
        rc = poll(&pfd, 1, 200);
        if (esf->should_stop)
            break;
        if (rc < 0)
        {
            CHUCHO_C_ERROR("Error waiting for DTrace data: %s", strerror(errno));
            break;
        }
        if (rc == 1)
        {

        }
    }
}

void destroy_event_source_impl(event_source* esrc)
{
    event_source_freebsd* esf;
    struct sglib_name_node_iterator itor;
    name_node* cur;

    esf = esrc->impl;
    esf->should_stop = true;
    yella_join_thread(esf->worker);
    yella_destroy_thread(esf->worker);
    yella_destroy_process(esf->dtrace);
    procstat_close(esf->pstat);
    for (cur = sglib_name_node_it_init(&itor, esf->names);
         cur != NULL;
         cur = sglib_name_node_it_next(&itor))
    {
        udsfree(cur->name);
        free(cur);
    }
    free(esf);
}

void init_event_source_impl(event_source* esrc)
{
    event_source_freebsd* esf;

    // TODO: error handling
    esrc->impl = malloc(sizeof(event_source_freebsd));
    esf = esrc->impl;
    esf->should_stop = false;
    esf->names = NULL;
    esf->pstat = procstat_open_sysctl();
    esf->dtrace = yella_create_process(u"<to do>");
    esf->worker = yella_create_thread(worker_main, esrc);
}
