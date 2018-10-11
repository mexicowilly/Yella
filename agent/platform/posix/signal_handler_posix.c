#include "agent/signal_handler.h"
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
    SIGTERM,
    -1
};

int term_signals[] =
{
    SIGQUIT,
    SIGXCPU,
    SIGXFSZ,
    SIGINT,
    SIGPIPE,
    SIGTERM,
    -1
};

int coring_signals[] =
{
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGILL,
    SIGSEGV,
    SIGSYS,
    SIGTRAP,
    -1
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
    { SIGCONT,   "SIGALRM" },
    { -1 }
};

sig_info* signal_info(int sig)
{
    static sig_info unknown = { -1, "UNKNOWN", "" };
    int i;
    sig_info* result;

    i = 0;
    while (info[i].sig != -1 && info[i].sig != sig)
        ++i;
    if(info[i].sig == -1)
    {
        unknown.sig = sig;
        result = &unknown;
    }
    else
    {
        info[i].description = strsignal(sig);
        result = &info[i];
    }
    return result;
}

static void log_signal(int signal)
{
    const sig_info* inf;

    inf = signal_info(signal);
    CHUCHO_C_INFO("yella.agent",
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
    i = 0;
    while (async_signals[i] != -1)
        sigaddset(&signals, i++);
    while (true)
    {
        sigwait(&signals, &recv_signal);
        log_signal(recv_signal);
        i = 0;
        while (term_signals[i] != -1 && term_signals[i] != recv_signal)
            ++i;
        if (term_signals[i] == recv_signal)
            term_handler(term_handler_udata);
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
    i = 0;
    while (async_signals[i] != -1)
        sigaddset(&blocked, async_signals[i++]);
    pthread_sigmask(SIG_BLOCK, &blocked, 0);
    pthread_create(&wait_thr, NULL, sigwait_thr, NULL);
    pthread_detach(wait_thr);
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = abort_handler;
    action.sa_flags = SA_RESETHAND;
    i = 0;
    while (coring_signals[i] != -1)
    {
        if (sigaction(coring_signals[i], &action, 0) != 0)
        {
            const sig_info* info = signal_info(i);
            printf("Failed installing signal handler for (%i) (%s) %s",
                   info->sig,
                   info->name,
                   info->description);
        }
        ++i;
    }
}

void set_signal_termination_handler(void (*hndlr)(void*), void* udata)
{
    term_handler = hndlr;
    term_handler_udata = udata;
}

