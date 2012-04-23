#ifndef SS_TABLE_H
#define SS_TABLE_H

/* SimpleShell, Process Table
 *
 * Authors:    Shane Tully and Gage Ames
 *
 * Interface information for a process table
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define ERROR -1

// Process state constants
#define STATE_NONE       0
#define STATE_RUNNING    1
#define STATE_TERMINATED 2
extern char *state[];

typedef struct child_process {
   pid_t pid;            /* process ID, supplied from fork() */
   int   state;          /* process state, your own definition */
   int   exit_status;    /* supplied from wait() if process has finished */
   char  *program;       /* program name*/

   struct child_process *next;
} child_process_t;

typedef struct process_table {
   int children;
   child_process_t *ptab;
} process_table_t;

/*----------------------------------------------------------------------------*/

/* process table for background processes */

int number_of_children(process_table_t *pt);

/* return NULL if not successful */
process_table_t *allocate_process_table(void);

/* ignore possible errors */
void deallocate_process_table(process_table_t *pt);

/* return 0 if successful, -1 if not */
int print_process_table(process_table_t *pt);
int insert_new_process(process_table_t *pt, pid_t pid, char *program);
int update_existing_process(process_table_t *pt, pid_t pid, int exit_status);
int remove_old_process(process_table_t *pt, pid_t pid);

/* wrappers */
char *Strdup(const char *s);
const char *safe_string(const char *str);

/*----------------------------------------------------------------------------*/ 

#endif
