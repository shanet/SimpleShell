#ifndef PR7_H
#define PR7_H

/* CMPSC 311 Project 7
 *
 * Authors:    Shane Tully and Gage Ames
 * Emails:     spt5063@psu.edu and gra5028@psu.edu
 *
 */

/*----------------------------------------------------------------------------*/

#define _GNU_SOURCE

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_INPUT 255
#define MAX_ARGS 128
#define STARTUP_FILE "pr7.init"

char *prog;
int  self_pid;
int  verbose;

extern char **environ;

/*
 *  MAX_LINE            input line length
 *  MAX_PATH            directory name length
 *  MAX_CHILDREN        number of child processes
 */

#ifdef _POSIX_C_SOURCE
/* use the minimal value for maximal portability */

#define MAX_LINE        _POSIX_MAX_INPUT
#define MAX_PATH        _POSIX_PATH_MAX
#define MAX_CHILDREN    _POSIX_CHILD_MAX

#else
/* use the default value for this system */

#define MAX_LINE        MAX_INPUT
#define MAX_PATH        PATH_MAX
#define MAX_CHILDREN    CHILD_MAX

/* Alternative versions can be obtained from
 *   #include <unistd.h>
 *   sysconf(_SC_LINE_MAX)
 *   sysconf(_SC_CHILD_MAX)
 */

#endif

#endif

int execvpe(const char *file, char *const argv[], char *const envp[]);
void usage(int status);                         /* print usage information */
int eval_line(char *cmdline);                   /* evaluate a command line */
int parse(char *buf, char *argv[]);             /* build the argv array */
int builtin(char *argv[]);                      /* if builtin command, run it */
