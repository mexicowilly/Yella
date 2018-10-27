#include "common/process.h"
#include "common/text_util.h"
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

typedef struct yella_process
{
    pid_t pid;
    FILE* in;
} yella_process;

yella_process* yella_create_process(const UChar* const command)
{
    char* shell;
    const char* argv[4];
    pid_t pid;
    yella_process* proc;
    int link[2];
    char* u8cmd;

    if (pipe(link) != 0)
        return NULL;
    proc = NULL;
    pid = fork();
    if (pid == -1)
    {
        close(link[0]);
        close(link[1]);
        return NULL;
    }
    else if (pid == 0)
    {
        dup2(link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);
        shell = getenv("SHELL");
        if (shell == NULL)
            shell = "sh";
        argv[0] = shell;
        argv[1] = "-c";
        u8cmd = yella_to_utf8(command);
        argv[2] = u8cmd;
        argv[3] = NULL;
        execvp(shell, (char* const*)argv);
        _exit(127);
    }
    else
    {
        close(link[1]);
        proc = malloc(sizeof(yella_process));
        proc->pid = pid;
        proc->in = fdopen(link[0], "r");
    }
    return proc;
}

void yella_destroy_process(yella_process* proc)
{
    pid_t pid;
    int rc;

    fclose(proc->in);
    if (kill(proc->pid, 0) == 0)
    {
        kill(proc->pid, SIGTERM);
        do
        {
            pid = waitpid(proc->pid, &rc, 0);
        } while (pid == -1 && errno == EINTR);
    }
    free(proc);
}

FILE* yella_process_get_reader(yella_process* proc)
{
    return proc->in;
}
