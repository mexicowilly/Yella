#include "plugin/file/event_source.h"
#include "plugin/file/file_name_matcher.h"
#include "common/process.h"
#include "common/text_util.h"
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
    UChar* name;
    const UChar* config_name;
    bool is_interesting;
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
    yella_mutex* guard;
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

static UChar* name_of_fd(pid_t pid, int fd, struct procstat* pstat, chucho_logger_t* lgr)
{
    struct kinfo_proc* kp;
    struct filestat_list* files;
    struct filestat* fst;
    UChar* result;
    unsigned cnt;

    result = NULL;
    kp = procstat_getprocs(pstat, KERN_PROC_PID, pid, &cnt);
    if (kp == NULL)
    {
        CHUCHO_C_ERROR(lgr, "Unable to look up PID: %d", pid);
    }
    else
    {
        files = procstat_getfiles(pstat, kp, 0);
        if (files == NULL)
        {
            CHUCHO_C_ERROR(lgr, "Unable to look up file list of PID: %d", pid);
        }
        else
        {
            STAILQ_FOREACH(fst, files, next)
            {
                if (fst->fs_type == PS_FST_TYPE_VNODE && fst->fs_fd == fd)
                {
                    result = yella_from_utf8(fst->fs_path);
                    break;
                }
            }
            procstat_freefiles(pstat, files);
        }
        procstat_freeprocs(pstat, kp);
    }
    return result;
}

static const UChar* file_name_matches_any(const event_source* const esrc, const UChar* const fname)
{
    int i;
    struct sglib_event_source_spec_iterator spec_itor;
    event_source_spec* cur_spec;
    const UChar* result;

    result = NULL;
    yella_read_lock_reader_writer_lock(esrc->guard);
    for (cur_spec = sglib_event_source_spec_it_init(&spec_itor, esrc->specs);
         cur_spec != NULL;
         cur_spec = sglib_event_source_spec_it_next(&spec_itor))
    {
        for (i = 0; i < yella_ptr_vector_size(cur_spec->excludes); i++)
        {
            if (file_name_matches(fname, yella_ptr_vector_at(cur_spec->excludes, i)))
            {
                yella_unlock_reader_writer_lock(esrc->guard);
                return result;
            }
        }
        for (i = 0; i < yella_ptr_vector_size(cur_spec->includes); i++)
        {
            if (file_name_matches(fname, yella_ptr_vector_at(cur_spec->includes, i)))
            {
                result = cur_spec->name;
                yella_unlock_reader_writer_lock(esrc->guard);
                return result;
            }
        }
    }
    yella_unlock_reader_writer_lock(esrc->guard);
    return result;
}

static void handle_close(event_source_freebsd* esf, const char* const line, chucho_logger_t* lgr)
{
    pid_t pid;
    int fd;
    int rc;
    name_node to_remove;
    name_node* removed;

    if (sscanf(line, "close:%d,%d\n", &pid, &fd) == 2)
    {
        to_remove.pid = pid;
        to_remove.fd = fd;
        yella_lock_mutex(esf->guard);
        if (sglib_name_node_delete_if_member(&esf->names, &to_remove, &removed))
        {
            free(removed->name);
            free(removed);
        }
        yella_unlock_mutex(esf->guard);
    }
    else
    {
        CHUCHO_C_ERROR(lgr, "Unable to parse line for close event: '%s'", line);
    }
}

static void handle_exit(event_source_freebsd* esf, const char* const line, chucho_logger_t* lgr)
{
    pid_t pid;
    struct sglib_name_node_iterator itor;
    name_node* cur;
    yella_ptr_vector* to_remove;
    int i;

    if (sscanf(line, "exit:%d\n", &pid) == 1)
    {
        to_remove = NULL;
        yella_lock_mutex(esf->guard);
        for (cur = sglib_name_node_it_init(&itor, esf->names);
             cur != NULL;
             cur = sglib_name_node_it_next(&itor))
        {
            if (cur->pid == pid)
            {
                free(cur->name);
                if (to_remove == NULL)
                    to_remove = yella_create_ptr_vector();
                yella_push_back_ptr_vector(to_remove, cur);
            }
        }
        if (to_remove != NULL)
        {
            for (i = 0; i < yella_ptr_vector_size(to_remove); i++)
                sglib_name_node_delete(&esf->names, yella_ptr_vector_at(to_remove, i));
            yella_destroy_ptr_vector(to_remove);
        }
        yella_unlock_mutex(esf->guard);
    }
    else
    {
        CHUCHO_C_ERROR(lgr, "Unable to parse line for exit event: '%s'", line);
    }
}

static void handle_write(const event_source* const esrc,
                         event_source_freebsd* esf,
                         const char* const line)
{
    pid_t pid;
    int fd;
    int rc;
    name_node to_find;
    name_node* found;
    UChar* name;

    rc = sscanf(line, "write:%d,%d\n", &pid, &fd);
    if (rc == 2)
    {
        to_find.pid = pid;
        to_find.fd = fd;
        yella_lock_mutex(esf->guard);
        found = sglib_name_node_find_member(esf->names, &to_find);
        if (found == NULL)
        {
            name = name_of_fd(pid, fd, esf->pstat, esrc->lgr);
            if (name == NULL)
            {
                return;
            }
            else
            {
                found = malloc(sizeof(name_node));
                found->pid = pid;
                found->fd = fd;
                found->config_name = file_name_matches_any(esrc, name);
                if (found->config_name == NULL)
                {
                    free(name);
                    found->name = NULL;
                    found->is_interesting = false;
                }
                else
                {
                    found->name = name;
                    found->is_interesting = true;
                }
                sglib_name_node_add(&esf->names, found);
            }
        }
        assert(found != NULL);
        if (found->is_interesting)
        {
            assert(found->name != NULL);
            assert(found->config_name != NULL);
            esrc->callback(found->config_name, found->name, esrc->callback_udata);
        }
        yella_unlock_mutex(esf->guard);
    }
    else
    {
        CHUCHO_C_ERROR(esrc->lgr, "Unable to parse line for write event: '%s'", line);
    }
}

static void worker_main(void* udata)
{
    event_source* esrc;
    event_source_freebsd* esf;
    FILE* reader;
    struct pollfd pfd;
    int rc;
    /* Possibilities are:
     * "write:<pid>,<fd>"
     * "close:<pid>,<fd>"
     * "exit:<pid>"
     */
    char line[6 + 512 + 1 + 512 + 1];
    pid_t pid;
    int fd;

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
            if (fgets(line, sizeof(line), reader) == NULL)
            {
                // TODO: error
            }
            assert(strlen(line) >= 10);
            if (strncmp(line, "write:", 6) == 0)
                handle_write(esrc, esf, line);
            else if (strncmp(line, "close:", 6) == 0)
                handle_close(esf, line, esrc->lgr);
            else if (strncmp(line, "exit:", 5) == 0)
                handle_exit(esf, line, esrc->lgr);
            else
                CHUCHO_C_ERROR("Invalid line from DTrace: '%s'", line);
        }
    }
}

/* The specs are write-locked on entry */
void clear_event_source_impl_specs(event_source* esrc)
{
    struct sglib_name_node_iterator itor;
    name_node* cur;
    event_source_freebsd* esf;

    esf = esrc->impl;
    yella_lock_mutex(esf->guard);
    for (cur = sglib_name_node_it_init(&itor, esf->names);
         cur != NULL;
         cur = sglib_name_node_it_next(&itor))
    {
        free(cur->name);
        free(cur);
    }
    esf->names = NULL;
    yella_unlock_mutex(esf->guard);
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
    clear_event_source_impl_specs(esrc);
    yella_destroy_mutex(esf->guard);
    free(esf);
}

void init_event_source_impl(event_source* esrc)
{
    event_source_freebsd* esf;

    // TODO: error handling
    esrc->impl = malloc(sizeof(event_source_freebsd));
    esf = esrc->impl;
    esf->guard = yella_create_mutex();
    esf->should_stop = false;
    esf->names = NULL;
    esf->pstat = procstat_open_sysctl();
    esf->dtrace = yella_create_process(u"<to do>");
    esf->worker = yella_create_thread(worker_main, esrc);
}

void remove_event_source_impl_spec(event_source* esrc, const UChar* const config_name)
{
    struct sglib_name_node_iterator itor;
    name_node* cur;
    event_source_freebsd* esf;
    yella_ptr_vector* to_remove;
    int i;

    esf = esrc->impl;
    to_remove = yella_create_ptr_vector();
    yella_lock_mutex(esf->guard);
    for (cur = sglib_name_node_it_init(&itor, esf->names);
         cur != NULL;
         cur = sglib_name_node_it_next(&itor))
    {
        if (cur->config_name != NULL && u_strcmp(cur->config_name, config_name) == 0)
            yella_push_back_ptr_vector(to_remove, cur);
    }
    for (i = 0; i < yella_ptr_vector_size(to_remove); i++)
    {
        cur = yella_ptr_vector_at(to_remove, i);
        free(cur->name);
        sglib_name_node_delete(&esf->names, cur);
    }
    yella_unlock_mutex(esf->guard);
    yella_destroy_ptr_vector(to_remove);
}
