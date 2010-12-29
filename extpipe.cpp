#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <vdr/tools.h>
#include <vdr/thread.h>
#include "extpipe.h"



cExtPipe::cExtPipe(void)
{
    pid = -1;
    f = NULL;
}

cExtPipe::~cExtPipe()
{
    int status;
    Close(status);
}

bool cExtPipe::Open(const char *Command, const char *Mode)
{
    int fd[2];

    if (pipe(fd) < 0)
    {
        LOG_ERROR;
        return false;
    }
    if ((pid = fork()) < 0)   // fork failed
    {
        LOG_ERROR;
        close(fd[0]);
        close(fd[1]);
        return false;
    }

    const char *mode = "w";
    int iopipe = 0;

    if (pid > 0)   // parent process
    {
        if (strcmp(Mode, "r") == 0)
        {
            mode = "r";
            iopipe = 1;
        }
        close(fd[iopipe]);
        if ((f = fdopen(fd[1 - iopipe], mode)) == NULL)
        {
            LOG_ERROR;
            close(fd[1 - iopipe]);
        }
        return f != NULL;
    }
    else   // child process
    {
        int iofd = STDOUT_FILENO;
        if (strcmp(Mode, "w") == 0)
        {
            mode = "r";
            iopipe = 1;
            iofd = STDIN_FILENO;
        }
        close(fd[iopipe]);
        if (dup2(fd[1 - iopipe], iofd) == -1)   // now redirect
        {
            LOG_ERROR;
            close(fd[1 - iopipe]);
            _exit(-1);
        }
        else
        {
            int MaxPossibleFileDescriptors = getdtablesize();
            for (int i = STDERR_FILENO + 1; i < MaxPossibleFileDescriptors; i++)
                close(i); //close all dup'ed filedescriptors
            if (execl("/bin/sh", "sh", "-c", Command, NULL) == -1)
            {
                LOG_ERROR_STR(Command);
                close(fd[1 - iopipe]);
                _exit(-1);
            }
        }
        _exit(0);
    }
}

int cExtPipe::Close(int &status)
{
    int ret = -1;

    if (f)
    {
        fclose(f);
        f = NULL;
    }

    if (pid > 0)
    {
        int i = 5;
        while (i > 0)
        {
            ret = waitpid(pid, &status, WNOHANG);
            if (ret < 0)
            {
                if (errno != EINTR && errno != ECHILD)
                {
                    LOG_ERROR;
                    break;
                }
            }
            else if (ret == pid)
                break;
            i--;
            cCondWait::SleepMs(100);
        }
        if (!i)
        {
            kill(pid, SIGKILL);
            ret = -1;
        }
        else if (ret == -1 || !WIFEXITED(status))
            ret = -1;
        pid = -1;
    }

    return ret;
}
