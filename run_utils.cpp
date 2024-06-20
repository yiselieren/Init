/*
 * Run utils
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

#include "run_utils.h"

#define NULL_FILE  "/dev/null"
#define CMDLEN     512
#define MAXARGS    50

////////////////////////////////////////////////////////////////////////
/*
 * Execute the specified command. Set stdout_fd and stderr_fd to stdout
 * and stderr of the target command. Caller may read from these fds
 * Set stdin_fd to stdin of the target command. Caller may write to it.
 */
bool exec_process(pid_t *pid, int *stdin_fd, int *stdout_fd, int *stderr_fd, int /*argc*/, char **argv)
{
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];

    if (pipe2(stdin_pipe, O_NONBLOCK) < 0)
        return false;
    if (pipe2(stdout_pipe, O_NONBLOCK) < 0)
        return false;
    if (stderr_fd)
    {
        if (pipe2(stderr_pipe, O_NONBLOCK) < 0)
            return false;
    }
    *pid = vfork();
    if (*pid)
    {
        /*
         * We are parent
         * -------------
         */
        if (stdin_fd)
            *stdin_fd = stdin_pipe[1];
        if (stdout_fd)
            *stdout_fd = stdout_pipe[0];
        if (stderr_fd)
            *stderr_fd = stderr_pipe[0];

        return true;
    }
    else
    {
        /*
         * We are child
         * ------------
         */


        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        dup2(stdin_pipe[0], 0);
        dup2(stdout_pipe[1], 1);
        if (stderr_fd)
            dup2(stderr_pipe[1], 2);
        else
            dup2(stdout_pipe[1], 2);

        // Close only used FDs
        DIR           *dir;
        struct dirent *ent;
        char          childs_dir[32];
        sprintf(childs_dir,"/proc/%d/fd", getpid());

        if ((dir = opendir (childs_dir)) != NULL)
        {
            while ((ent = readdir (dir)) != NULL) {
                char* end;
                int fd = strtol(ent->d_name, &end, 32);
                if (!*end && fd > STDERR_FILENO)
                    close(fd);
            }
            closedir (dir);
        }

        execvp(argv[0], argv);

        printf("%s: %s\n", argv[0], (errno == ENOENT) ? "Bad command or file name" : strerror(errno));
        _exit(0);
    }
    return false;
} // runcmd
