/* CMPSC 311 Project 7, Process Table
 *
 * Authors:    Shane Tully and Gage Ames
 * Emails:     spt5063@psu.edu and gra5028@psu.edu
 *
 * Interface information for a process table
 */

#include "pr7_table.h"

// typedef struct child_process {
//    pid_t pid;            /* process ID, supplied from fork() */
//    int   state;          /* process state, your own definition */
//    int   exit_status;    /* supplied from wait() if process has finished */

//    struct child_process *next;
// } child_process_t;

// typedef struct process_table {
//    int children;
//    child_process_t *ptab;
// } process_table_t;

/*----------------------------------------------------------------------------*/

/* process table for background processes */

int number_of_children(process_table_t *pt) {
   if(pt != NULL) {
      return -1;
   }

   return pt->children;
}

/* return NULL if not successful */
process_table_t *allocate_process_table(void) {
   process_table_t *ptab = malloc(sizeof(process_table_t));
   if (ptab == NULL) {
      return NULL;
   }

   ptab->children = 0;
   ptab->ptab = NULL;

   return ptab;
}

/* ignore possible errors */
void deallocate_process_table(process_table_t *pt) {
   if (pt == NULL) {
      return;
   }

   child_process_t *cur;
   child_process_t *prev = NULL;
   for (cur = pt->ptab; cur != NULL; cur = cur->next)
   {
      free(prev);
      prev = cur;
   }

   free(prev);
   free(pt);

   return;
}

/* return 0 if successful, -1 if not */
int print_process_table(process_table_t *pt) {
   if (pt == NULL) {
      return -1;
   }

   printf("  process table\n");
   printf("       pid         state        status\n");

   child_process_t *child;
   for (child = pt->ptab; child != NULL; child = child->next) {
      printf("    %6d    %10s    0x%08x\n", child->pid, state[child->state],
             child->exit_status);
   }

   return 0;
}

int insert_new_process(process_table_t *pt, pid_t pid) {
   if (pt == NULL) {
      return -1;
   }

   child_process_t *new;
   new = malloc(sizeof(child_process_t));
   new->pid = pid;
   new->state = STATE_RUNNING;
   new->exit_status = 0;
   new->next = NULL;

   if (pt->ptab == NULL) {    // First entry in the list
      pt->ptab = new;
   } else {                   // Append to the end of the list
      child_process_t *end;
      for (end = pt->ptab; end->next != NULL; end = end->next);
      end->next = new;
   }

   pt->children++;
   return 0;
}

int update_existing_process(process_table_t *pt, pid_t pid, int exit_status) {
   if (pt == NULL) {
      return -1;
   }

   child_process_t *target = pt->ptab;
   while (target != NULL && target->pid != pid) {
      target = target->next;
   }

   if (target == NULL) {      // The process with PID pid was not found
      return -1;
   } else {                   // Change the exit status of the process
      target->exit_status = exit_status;
   }

   return 0;
}

int remove_old_process(process_table_t *pt, pid_t pid) {
   if (pt == NULL) {
      return -1;
   }

   child_process_t *prev = NULL;
   child_process_t *target = pt->ptab;
   while (target != NULL && target->pid != pid) {
      prev = target;
      target = target->next;
   }

   if (target == NULL) {      // The process with PID pid was not found
      return -1;
   } else if (prev == NULL) { // The process is the first entry in the list
      pt->ptab = target->next;
   } else {                   // Remove the process from the list
      prev->next = target->next;
   }

   // Mark the item as terminated and free it
   target->state = STATE_TERMINATED;
   free(target);

   pt->children--;
   return 0;
}

/*----------------------------------------------------------------------------*/ 
