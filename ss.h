#ifndef SS_H
#define SS_H

/* SimpleShell
 *
 * Authors:    Shane Tully and Gage Ames
 *
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "ss_table.h"

#define MAX_COMMAND 4096
#define MAX_ARGS 128
#define STARTUP_FILE "pr7.init"

#define EXEC_LP 0
#define EXEC_VP 1
#define EXEC_VE 2

// Types of redirection
#define R_NONE      0
#define R_STDIN     1
#define R_STDOUT    2
#define R_BOTH      3

// MAX_LINE            input line length
// MAX_PATH            directory name length
// MAX_CHILDREN        number of child processes

#ifdef _POSIX_C_SOURCE
/* use the minimal value for maximal portability */
#define MAX_LINE        _POSIX_MAX_INPUT
#define MAX_PATH        _POSIX_PATH_MAX
#define MAX_CHILDREN    _POSIX_CHILD_MAX

#else
/* use the default value for this system */
#define MAX_LINE        255
#define MAX_PATH        PATH_MAX
#define MAX_CHILDREN    CHILD_MAX

#endif

// eru.cse.psu.edu's signal.h does not define sighandler_t;
// define it here to be safe
typedef void (*sighandler_t) (int);

char *prog;
char *ps1;
char *ps2;
int self_pid;
int verbose;
int debug;
int exec;
int echo;
process_table_t *ptable;
static pid_t foreground_pid = 0;
extern char **environ;

void usage(int status);                         /* print usage information */
int eval_line(char *cmdline);                   /* evaluate a command line */
int parse(char *buf, char *argv[]);             /* build the argv array */
int builtin(char *argv[]);                      /* if builtin command, run it */
int redirect(char *argv[]);
char* get_prompt(int isLineCont);
void update_prompt(int isLineCont);
void Exit(int status);
void print_wait_status(pid_t pid, int status);
void cleanup_terminated_children(void);
void SIGINT_handler(int sig);
int install_signal_handler(int sig, sighandler_t func);
int ignore_signal_handler(int sig);

#endif
