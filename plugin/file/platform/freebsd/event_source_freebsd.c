#include "plugin/file/event_source.h"
#include "plugin/file/file_name_matcher.h"
#include "common/process.h"
#include "common/text_util.h"
#include "common/settings.h"
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

typedef struct pending_line
{
    char* line;
    struct pending_line* previous;
    struct pending_line* next;
} pending_line;

typedef struct event_source_freebsd
{
    yella_process* dtrace;
    yella_thread* worker;
    atomic_bool should_stop;
    struct procstat* pstat;
    name_node* names;
    yella_mutex* guard;
    struct
    {
        pending_line* lines;
        pending_line* last;
        size_t line_count;
        yella_mutex* guard;
        yella_condition_variable* full_cond;
        yella_condition_variable* empty_cond;
    } pending;
    yella_thread* reader;
} event_source_freebsd;

static int name_node_comparator(name_node* lhs, name_node* rhs)
{
    int result;

    result = lhs->pid - rhs->pid;
    if (result == 0)
        result = lhs->fd - rhs->fd;
    return result;
}

static void name_node_destructor(void* nn, void* udata)
{
    name_node* cur;

    cur = (name_node*)nn;
    free(cur->name);
    free(cur);
}

SGLIB_DEFINE_RBTREE_PROTOTYPES(name_node, left, right, color, name_node_comparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(name_node, left, right, color, name_node_comparator);

#define PENDING_COMPARATOR(lhs, rhs) (strcmp(lhs->line, rhs->line))

SGLIB_DEFINE_DL_LIST_PROTOTYPES(pending_line, PENDING_COMPARATOR, previous, next);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(pending_line, PENDING_COMPARATOR, previous, next);

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
                goto top_break;
        }
        for (i = 0; i < yella_ptr_vector_size(cur_spec->includes); i++)
        {
            if (file_name_matches(fname, yella_ptr_vector_at(cur_spec->includes, i)))
            {
                result = cur_spec->name;
                goto top_break;
            }
        }
    }
    top_break:
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
            name_node_destructor(removed, NULL);
        yella_unlock_mutex(esf->guard);
    }
    else
    {
        CHUCHO_C_ERROR(lgr, "Unable to parse line for close event: '%s'", line);
    }
}

static void handle_closefrom(event_source_freebsd* esf, const char* const line, chucho_logger_t* lgr)
{
    pid_t pid;
    int fd;
    struct sglib_name_node_iterator itor;
    name_node* cur;
    yella_ptr_vector* to_remove;
    int i;

    if (sscanf(line, "closefrom:%d,%d\n", &pid, &fd) == 2)
    {
        to_remove = NULL;
        yella_lock_mutex(esf->guard);
        for (cur = sglib_name_node_it_init(&itor, esf->names);
             cur != NULL;
             cur = sglib_name_node_it_next(&itor))
        {
            if (cur->pid == pid && cur->fd >= fd)
            {
                if (to_remove == NULL)
                {
                    to_remove = yella_create_ptr_vector();
                    yella_set_ptr_vector_destructor(to_remove, name_node_destructor, NULL);
                }
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
        CHUCHO_C_ERROR(lgr, "Unable to parse line for closefrom event: '%s'", line);
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
                if (to_remove == NULL)
                {
                    to_remove = yella_create_ptr_vector();
                    yella_set_ptr_vector_destructor(to_remove, name_node_destructor, NULL);
                }
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

static void reader_main(void* udata)
{
    event_source* esrc;
    event_source_freebsd* esf;
    FILE* reader;
    struct pollfd pfd;
    int rc;
    /* Possibilities are:
     * "write:<pid>,<fd>"
     * "close:<pid>,<fd>"
     * "closefrom:<pid>,<fd>"
     * "exit:<pid>"
     */
    char line[10 + 512 + 1 + 512 + 1];
    pid_t pid;
    int fd;
    size_t len;
    size_t max_events;
    pending_line* to_add;
    bool was_waiting;

    esrc = udata;
    esf = esrc->impl;
    reader = yella_process_get_reader(esf->dtrace);
    pfd.fd = fileno(reader);
    pfd.events = POLLIN;
    max_events = *yella_settings_get_uint(u"file", u"max-events-in-cache");
    while (!esf->should_stop)
    {
        was_waiting = false;
        yella_lock_mutex(esf->pending.guard);
        while (!esf->should_stop && esf->pending.line_count >= max_events)
            was_waiting = yella_wait_milliseconds_for_condition_variable(esf->pending.full_cond, esf->pending.guard, 250);
        yella_unlock_mutex(esf->pending.guard);
        if (esf->should_stop)
            break;
        rc = poll(&pfd, 1, 250);
        if (esf->should_stop)
            break;
        if (rc < 0)
        {
            CHUCHO_C_ERROR(esrc->lgr, "Error waiting for DTrace data: %s", strerror(errno));
            break;
        }
        if (rc == 1)
        {
            if (was_waiting)
                CHUCHO_C_WARN(esrc->lgr, "The event queue was full and is now emptying");
            if (fgets(line, sizeof(line), reader) == NULL)
            {
                if (yella_process_is_running(esf->dtrace))
                {
                    CHUCHO_C_ERROR(esrc->lgr, "Error reading from DTrace stream");
                    clearerr(reader);
                }
                else
                {
                    yella_destroy_process(esf->dtrace);
                    CHUCHO_C_ERROR(esrc->lgr, "DTrace has exited and will be restarted");
                    esf->dtrace = yella_create_process(u"<to do>");
                    if (esf->dtrace == NULL)
                    {
                        CHUCHO_C_FATAL(esrc->lgr, "Unable to start DTrace. Exiting the worker thread...");
                        break;
                    }
                    reader = yella_process_get_reader(esf->dtrace);
                }
                continue;
            }
            to_add = malloc(sizeof(pending_line));
            to_add->line = strdup(line);
            yella_lock_mutex(esf->pending.guard);
            sglib_pending_line_add_after(&esf->pending.last, to_add);
            if (esf->pending.last->next != NULL)
                esf->pending.last = esf->pending.last->next;
            if (esf->pending.lines == NULL)
                esf->pending.lines = esf->pending.last;
            if (++esf->pending.line_count == 1)
                yella_signal_condition_variable(esf->pending.empty_cond);
            yella_unlock_mutex(esf->pending.guard);
        }
    }
}

static void worker_main(void* udata)
{
    event_source* esrc;
    event_source_freebsd* esf;
    size_t max_events;
    pending_line* cur;
    size_t len;

    esrc = udata;
    esf = esrc->impl;
    // Minus one because this is the test for whether to signal the full condition
    max_events = *yella_settings_get_uint(u"file", u"max-events-in-cache") - 1;
    while (!esf->should_stop)
    {
        yella_lock_mutex(esf->pending.guard);
        while (!esf->should_stop && esf->pending.line_count == 0)
            yella_wait_milliseconds_for_condition_variable(esf->pending.empty_cond, esf->pending.guard, 250);
        yella_unlock_mutex(esf->pending.guard);
        if (esf->should_stop)
            break;
        yella_lock_mutex(esf->pending.guard);
        cur = esf->pending.lines;
        assert(cur != NULL);
        sglib_pending_line_delete(&esf->pending.lines, cur);
        if (--esf->pending.line_count == max_events)
            yella_signal_condition_variable(esf->pending.full_cond);
        yella_unlock_mutex(esf->pending.guard);
        len = strlen(cur->line);
        if (len > 6 && strncmp(cur->line, "write:", 6) == 0)
            handle_write(esrc, esf, cur->line);
        else if (len > 6 && strncmp(cur->line, "close:", 6) == 0)
            handle_close(esf, cur->line, esrc->lgr);
        else if (len > 5 && strncmp(cur->line, "exit:", 5) == 0)
            handle_exit(esf, cur->line, esrc->lgr);
        else if (len > 10 && strncmp(cur->line, "closefrom:", 10) == 0)
            handle_closefrom(esf, cur->line, esrc->lgr);
        else
            CHUCHO_C_ERROR(esrc->lgr, "Invalid line from DTrace: '%s'", cur->line);
        free(cur->line);
        free(cur);
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
    yella_join_thread(esf->reader);
    yella_destroy_thread(esf->reader);
    yella_destroy_process(esf->dtrace);
    yella_destroy_condition_variable(esf->pending.empty_cond);
    yella_destroy_condition_variable(esf->pending.full_cond);
    yella_destroy_mutex(esf->pending.guard);
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
    esf->pending.guard = yella_create_mutex();
    esf->pending.empty_cond = yella_create_condition_variable();
    esf->pending.full_cond = yella_create_condition_variable();
    esf->dtrace = yella_create_process(u"<to do>");
    esf->reader = yella_create_thread(reader_main, esrc);
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
