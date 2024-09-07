/*
 * Misc. run related utilities
 */
#ifndef RUN_UTILS_H
#define RUN_UTILS_H
#include <unistd.h>

#include <unistd.h>

bool exec_process(pid_t *pid, int *stdin_fd, int *stdout_fd, int *stderr_fd, const char *wd, int argc, char **argv);
bool exec_process_pty(pid_t *pid, int *master_fd, const char *wd, int argc, char **argv);

#endif

