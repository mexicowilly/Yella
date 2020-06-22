#include "agent/signal_handler.h"
#include "common/macro_util.h"
#include <chucho/log.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

int async_signals[] =
{
    SIGHUP,
    SIGUSR1,
    SIGUSR2,
    SIGCHLD,
    SIGVTALRM,
    SIGQUIT,
    SIGXCPU,
    SIGXFSZ,
    SIGINT,
    SIGPIPE,
    SIGTERM
};

int term_signals[] =
{
    SIGQUIT,
    SIGXCPU,
    SIGXFSZ,
    SIGINT,
    SIGPIPE,
    SIGTERM
};

int coring_signals[] =
{
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGILL,
    SIGSEGV,
    SIGSYS,
    SIGTRAP
};

sig_info info[] =
{
    { SIGHUP,    "SIGHUP" },
    { SIGUSR1,   "SIGUSR1" },
    { SIGUSR2,   "SIGUSR2" },
    { SIGCHLD,   "SIGCHLD" },
    { SIGVTALRM, "SIGVTALRM" },
    { SIGQUIT,   "SIGQUIT" },
    { SIGXCPU,   "SIGXCPU" },
    { SIGXFSZ,   "SIGXFSZ" },
    { SIGINT,    "SIGINT" },
    { SIGPIPE,   "SIGPIPE" },
    { SIGTERM,   "SIGTERM" },
    { SIGABRT,   "SIGABRT" },
    { SIGBUS,    "SIGBUS" },
    { SIGFPE,    "SIGFPE" },
    { SIGILL,    "SIGILL" },
    { SIGSEGV,   "SIGSEGV" },
    { SIGSYS,    "SIGSYS" },
    { SIGTRAP,   "SIGTRAP" },
    { SIGALRM,   "SIGALRM" },
    { SIGIO,     "SIGIO" },
    { SIGIOT,    "SIGIOT" },
    { SIGPROF,   "SIGPROF" },
    { SIGURG,    "SIGURG" },
    { SIGWINCH,  "SIGWINCH" },
    { SIGSTOP,   "SIGSTOP" },
    { SIGTSTP,   "SIGTSTOP" },
    { SIGTTIN,   "SIGTTIN" },
    { SIGTTOU,   "SIGTTOU" },
    { SIGCONT,   "SIGALRM" }
};

sig_info* signal_info(int sig)
{
    static sig_info unknown = { -1, "UNKNOWN", "" };
    int i;
    sig_info* result;

    for (i = 0; i < YELLA_ARRAY_SIZE(info); i++)
    {
        if (info[i].sig == sig)
        {
            info[i].description = strsignal(sig);
            result = &info[i];
            break;
        }
    }
    if (i == YELLA_ARRAY_SIZE(info))
    {
        unknown.sig = sig;
        result = &unknown;
    }
    return result;
}

static void log_signal(int signal)
{
    const sig_info* inf;

    inf = signal_info(signal);
    CHUCHO_C_INFO("agent",
                  "Signal (%i) (%s) %s",
                  inf->sig,
                  inf->name,
                  inf->description);
}

static void abort_handler(int signal)
{
    log_signal(signal);
    raise(signal);
}

static void default_term_handler(void* udata)
{
    exit(EXIT_FAILURE);
};

static void (*term_handler)(void*) = default_term_handler;
static void* term_handler_udata = NULL;

static void* sigwait_thr(void* arg)
{
    sigset_t signals;
    int i;
    int recv_signal;

    sigemptyset(&signals);
    for (i = 0; i < YELLA_ARRAY_SIZE(async_signals); i++)
        sigaddset(&signals, async_signals[i]);
    while (true)
    {
        sigwait(&signals, &recv_signal);
        log_signal(recv_signal);
        for (i = 0; i < YELLA_ARRAY_SIZE(term_signals); i++)
        {
            if (term_signals[i] == recv_signal)
            {
                term_handler(term_handler_udata);
                break;
            }
        }
        sigaddset(&signals, recv_signal);
    }
}

void install_signal_handler(void)
{
    int i;
    sigset_t blocked;
    pthread_t wait_thr;
    struct sigaction action;

    sigemptyset(&blocked);
    for (i = 0; i < YELLA_ARRAY_SIZE(async_signals); i++)
        sigaddset(&blocked, async_signals[i]);
    pthread_sigmask(SIG_BLOCK, &blocked, 0);
    pthread_create(&wait_thr, NULL, sigwait_thr, NULL);
    pthread_detach(wait_thr);
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = abort_handler;
    action.sa_flags = SA_RESETHAND;
    for (i = 0; i < YELLA_ARRAY_SIZE(coring_signals); i++)
    {
        if (sigaction(coring_signals[i], &action, 0) != 0)
        {
            const sig_info* info = signal_info(i);
            printf("Failed installing signal handler for (%i) (%s) %s",
                   info->sig,
                   info->name,
                   info->description);
        }
    }
}

void set_signal_termination_handler(void (*hndlr)(void*), void* udata)
{
    term_handler = hndlr;
    term_handler_udata = udata;
}

