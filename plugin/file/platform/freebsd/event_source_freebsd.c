#include "plugin/file/event_source.h"
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

typedef struct event_source_freebsd
{
    yella_process* dtrace;
    yella_thread* worker;
    atomic_bool should_stop;
    struct procstat* pstat;
} event_source_freebsd;

static char* cwd_of_pid(pid_t pid, struct procstat* pstat, chucho_logger_t* lgr)
{
    struct kinfo_proc* kp;
    struct filestat_list* files;
    struct filestat* fst;
    char* result;

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
    return result;
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

    esf = esrc->impl;
    esf->should_stop = true;
    yella_join_thread(esf->worker);
    yella_destroy_thread(esf->worker);
    yella_destroy_process(esf->dtrace);
    procstat_close(esf->pstat);
    free(esf);
}

void init_event_source_impl(event_source* esrc)
{
    event_source_freebsd* esf;

    esrc->impl = malloc(sizeof(event_source_freebsd));
    esf = esrc->impl;
    esf->should_stop = false;
    esf->pstat = procstat_open_sysctl();
    esf->dtrace = yella_create_process(u"<to do>");
    esf->worker = yella_create_thread(worker_main, esrc);
}
