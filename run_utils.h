/*
 * Misc. run related utilities
 */
#ifndef RUN_UTILS_H
#define RUN_UTILS_H
#include <unistd.h>

#include <unistd.h>

bool exec_process(pid_t *pid, int *stdin_fd, int *stdout_fd, int *stderr_fd, int argc, char **argv);

#endif

