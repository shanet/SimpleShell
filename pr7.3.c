/* CMPSC 311 Project 7, version 3
 *
 * Authors:    Shane Tully and Gage Ames
 * Emails:     spt5063@psu.edu and gra5028@psu.edu
 *
 * Usage:
 *   c99 -v -o pr7 pr7.2.c                        [Sun compiler]
 *   gcc -std=c99 -Wall -Wextra -o pr7 pr7.2.c    [GNU compiler]
 *
 *   pr7
 *   pr7%      [type a command and then return]
 *   pr7% exit
 *
 * This version is derived from the shellex.c program in CS:APP Sec. 8.4.
 */

/*----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include "pr7.h"
#include "pr7_table.h"

#define MAXLINE 128
#define MAXARGS 128

#define STARTUP_FILE "pr7.init"

// Function Prototypes
int execvpe(const char *file, char *const argv[], char *const envp[]);
void usage(int status);	                        /* print usage information */
int eval_line(char *cmdline);                   /* evaluate a command line */
int parse(char *buf, char *argv[]);             /* build the argv array */
int builtin(char *argv[]);                      /* if builtin command, run it */

extern char **environ;

/*----------------------------------------------------------------------------*/

void usage(int status) {
   if (status == EXIT_SUCCESS) {
      printf("Usage: %s [-h] [-v] [-i] [-e] [-s f] [file]\n", prog);
      printf("    -h      help\n");
      printf("    -i      interactive mode\n");
      printf("    -e      echo commands before execution\n");
      printf("    -s f    use startup file f, default pr7.init\n");

      printf("Shell commands:\n");
      printf("  exit help\n");
   } else {
      fprintf(stderr, "%s: Try '%s -h' for usage information.\n", prog, prog);
   }

   exit(status);
}

/*----------------------------------------------------------------------------*/

/* Compare to main() in CS:APP Fig. 8.22 */

int main(int argc, char *argv[]) {
   int ret = EXIT_SUCCESS;
   char cmdline[MAXLINE];                /* command line */

   prog = argv[0];
   self_pid = (int) getpid();

   // for use with getopt(3)
   int ch;
   extern char *optarg;
   extern int optind;
   extern int optopt;
   extern int opterr;

   // command line arguments
   verbose = 0;
   int interactive = 0;
   int echo = 0;
   int custom_startup = 0;
   char *startup_file = STARTUP_FILE;

   while((ch = getopt(argc, argv, ":hvies:")) != -1) {
      switch(ch) {
         case 'h':
            usage(EXIT_SUCCESS);
            break;
         case 'v':
            verbose = 1;
            break;
         case 'i':
            interactive = 1;
            break;
         case 'e':
            echo = 1;
            break;
         case 's':
            custom_startup = 1;
            startup_file = optarg;
            break;
         case '?':
            fprintf(stderr, "%s: invalid option '%c'\n", prog, optopt);
            usage(EXIT_FAILURE);
            break;
         case ':':
            fprintf(stderr, "%s: option '%c' missing argument\n", prog, optopt);
            usage(EXIT_FAILURE);
            break;
         default:
            usage(EXIT_FAILURE);
            break;
      }
   }

   if (verbose) {
      printf("%s %d: hello, world\n", prog, self_pid);
   }

   // Startup file
   FILE *startup = fopen(startup_file, "r");
   if (startup != NULL) {
      while (fgets(cmdline, MAXLINE, startup) != NULL) {
         ret = eval_line(cmdline);
      }

      if (ferror(startup) && custom_startup) {
         fprintf(stderr, "%s: error reading file %s: %s\n", prog, startup_file,
                 strerror(errno));
      }

      if (fclose(startup) != 0) {
         fprintf(stderr, "%s: cannot close file %s: %s\n", prog, startup_file,
                 strerror(errno));
      }
      startup = NULL;
   } else if (custom_startup) {
      fprintf(stderr, "%s: cannot open startup file %s: %s\n", prog,
              startup_file, strerror(errno));
   }

   // Input file
   FILE *infile;    
   char *infile_name;
   if (argv[optind] == NULL) {
      infile = stdin; infile_name = "[stdin]";
   } else {
      infile_name = argv[optind];  
      infile = fopen(infile_name, "r");  /* read-only */
      if (infile == NULL) {
         fprintf(stderr, "%s: cannot open file %s: %s\n", prog, infile_name,
                 strerror(errno));
         exit(EXIT_FAILURE);
      }
   }

   while (1) {
      /* issue prompt and read command line */
      if (interactive) {
         printf("%s%% ", prog);
      }

      fgets(cmdline, MAXLINE, infile);   /* cmdline includes trailing newline */
      if (ferror(infile)) {
         fprintf(stderr, "%s: error reading file %s: %s\n", prog, infile_name,
                 strerror(errno));
         break;
      }
      if (feof(infile)) {                /* end of file */
         break;
      }

      ret = eval_line(cmdline);
   }

   if (fclose(infile) != 0) {
      fprintf(stderr, "%s: cannot close file %s: %s\n", prog, startup_file,
             strerror(errno));
   }
   infile = NULL;

   return ret;
}

/*----------------------------------------------------------------------------*/

/* evaluate a command line
 *
 * Compare to eval() in CS:APP Fig. 8.23.
 */

int eval_line(char *cmdline) {
   char *argv[MAXARGS];  /* argv for execve() */
   char buf[MAXLINE];    /* holds modified command line */
   int background;       /* should the job run in background or foreground? */
   pid_t pid;            /* process id */
   int ret = EXIT_SUCCESS;

   strcpy(buf, cmdline);                 /* buf[] will be modified by parse() */
   background = parse(buf, argv);        /* build the argv array */

   if (argv[0] == NULL) {       /* ignore empty lines */
      return ret;
   }

   if (builtin(argv) == 1) {     /* the work is done */
      return ret;
   }

   if ((pid = fork()) == 0) {    /* child runs user job */
      if (execvpe(argv[0], argv, environ) == -1) {
         printf("%s: failed: %s\n", argv[0], strerror(errno));
         _exit(EXIT_FAILURE);
      }
   }

   if (background) {          /* parent waits for foreground job to terminate */
      printf("background process %d: %s", (int) pid, cmdline);
   } else {
      if (waitpid(pid, &ret, 0) == -1) {
         printf("%s: failed: %s\n", argv[0], strerror(errno));
         exit(EXIT_FAILURE);
      }
   }

   return ret;
}

/*----------------------------------------------------------------------------*/

/* parse the command line and build the argv array
 *
 * Compare to parseline() in CS:APP Fig. 8.24.
 */

int parse(char *buf, char *argv[]) {
   char *delim;          /* points to first whitespace delimiter */
   int argc = 0;         /* number of args */
   int bg;               /* background job? */

   char whsp[] = " \t\n\v\f\r";          /* whitespace characters */

  /* Note - the trailing '\n' in buf is whitespace, and we need it as a delimiter. */

   while (1) {                          /* build the argv list */
      buf += strspn(buf, whsp);         /* skip leading whitespace */
      delim = strpbrk(buf, whsp);       /* next whitespace char or NULL */
      if (delim == NULL) {              /* end of line */
         break;
      }
      argv[argc++] = buf;               /* start argv[i] */
      *delim = '\0';                    /* terminate argv[i] */
      buf = delim + 1;                  /* start argv[i+1]? */
   }
   argv[argc] = NULL;

   if (argc == 0) {                     /* blank line */
      return 0;
   }

   /* should the job run in the background? */
   if ((bg = (strcmp(argv[argc-1], "&") == 0))) {
      argv[--argc] = NULL;
   }

   return bg;
}

/*----------------------------------------------------------------------------*/

/* if first arg is a builtin command, run it and return true
 *
 * Compare to builtin_command() in CS:APP Fig. 8.23.
 */

int builtin(char *argv[]) {
   if (strcmp(argv[0], "exit") == 0) {  /* exit command */
      if (verbose) {
         printf("%s %d: goodbye, world\n", prog, self_pid);
      }
      exit(0);
   }

   if (strcmp(argv[0], "&") == 0) {     /* ignore singleton & */
      return 1;
   }

   return 0;                            /* not a builtin command */
}

/*----------------------------------------------------------------------------*/

