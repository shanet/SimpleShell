/* CMPSC 311 Project 7, version 3
 *
 * Authors:    Shane Tully and Gage Ames
 * Emails:     spt5063@psu.edu and gra5028@psu.edu
 */

#include "pr7.h"

int main(int argc, char *argv[]) {
   int status = EXIT_SUCCESS;
   char cmdline[MAX_LINE];

   prog = argv[0];
   self_pid = (int) getpid();

   // Command line args parsing variables
   int ch;
   extern char *optarg;
   extern int optind;
   extern int optopt;
   extern int opterr;

   verbose = 0;
   echo = 0;
   int interactive = 1;
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

   // Set up the process table
   ptable = allocate_process_table();

   if(verbose) {
      printf("%s %d: hello, world\n", prog, self_pid);
   }

   // Startup file
   FILE *startup = fopen(startup_file, "r");
   if(startup != NULL) {
      while(fgets(cmdline, MAX_LINE, startup) != NULL) {
         status = eval_line(cmdline);
      }

      if(ferror(startup) && custom_startup) {
         fprintf(stderr, "%s: error reading file %s: %s\n", prog, startup_file,
                 strerror(errno));
      }

      if(fclose(startup) != 0) {
         fprintf(stderr, "%s: cannot close file %s: %s\n", prog, startup_file,
                 strerror(errno));
      }
      startup = NULL;
   } else if(custom_startup) {
      fprintf(stderr, "%s: cannot open startup file %s: %s\n", prog,
              startup_file, strerror(errno));
   }

   // Input file
   FILE *infile;    
   char *infile_name;
   if(argv[optind] == NULL) {
      infile = stdin;
      infile_name = "[stdin]";
   } else {
      infile_name = argv[optind];  
      infile = fopen(infile_name, "r");
      if(infile == NULL) {
         fprintf(stderr, "%s: cannot open file %s: %s\n", prog, infile_name,
                 strerror(errno));
         exit(EXIT_FAILURE);
      }
   }

   // Main input loop
   while(1) {
      // If interctive mode, show a prompt
      if(interactive) {
         // Show a prompt in the format of [user]@[hostname]$
         // If user or hostname cannot be determined, fallback to the process name
         // as the prompt
         char *user;
         char host[_MAX_INPUT];
         if((user = getenv("USER")) != NULL && gethostname(host, _MAX_INPUT) == 0) {
            printf("%s@%s$ ", user, host);
         } else {
            printf("%s$ ", prog);
         }
      }

      // Read command from input
      // cmdline includes trailing newline
      fgets(cmdline, MAX_LINE, infile);

      if(ferror(infile)) {
         fprintf(stderr, "%s: error reading file %s: %s\n", prog, infile_name,
                 strerror(errno));
         break;
      }

      // end of file
      if(feof(infile)) {
         break;
      }

      status = eval_line(cmdline);
   }

   if(fclose(infile) != 0) {
      fprintf(stderr, "%s: cannot close file %s: %s\n", prog, startup_file,
             strerror(errno));
   }
   infile = NULL;

   deallocate_process_table(ptable);

   return status;
}

/*----------------------------------------------------------------------------*/

/* evaluate a command line
 *
 * Compare to eval() in CS:APP Fig. 8.23.
 */

int eval_line(char *cmdline) {
   char *argv[MAX_ARGS];                 /* argv for execve() */
   char buf[MAX_LINE];                   /* holds modified command line */
   int background;                       /* should the job run in background or foreground? */
   pid_t pid;                            /* process id */
   int status = EXIT_SUCCESS;

   strcpy(buf, cmdline);                 /* buf[] will be modified by parse() */
   background = parse(buf, argv);        /* build the argv array */

   // Ignore empty lines
   if(argv[0] == NULL) {
      return status;
   }

   // If echo is on, print out the command
   if(echo) {
      printf("%s", cmdline);
   }

   // Check for builtin commands (exit, echo, etc.)
   if(builtin(argv) == 0) {
      return status;
   }

   // Create child to exexute command
   if((pid = fork()) == 0) {
      if(execvp(argv[0], argv) == -1) {
         printf("%s: failed: %s\n", argv[0], strerror(errno));
         _exit(EXIT_FAILURE);
      }
   }

   // Parent waits for foreground job to terminate
   if(background) {
      printf("background process %d: %s", (int) pid, cmdline);
      //TODO
   } else {
      if(waitpid(pid, &status, 0) == -1) {
         printf("%s: failed: %s\n", argv[0], strerror(errno));
         exit(EXIT_FAILURE);
      }
   }

   return status;
}

/*----------------------------------------------------------------------------*/

// Parse the command line and build the argv array
int parse(char *buf, char *argv[]) {
   char *delim;          /* points to first whitespace delimiter */
   int argc = 0;         /* number of args */
   int bg;               /* background job? */

   char whsp[] = " \t\n\v\f\r";          /* whitespace characters */

   /* Note - the trailing '\n' in buf is whitespace, and we need it as a delimiter. */

   while(1) {                          /* build the argv list */
      buf += strspn(buf, whsp);        /* skip leading whitespace */
      delim = strpbrk(buf, whsp);      /* next whitespace char or NULL */
      if(delim == NULL) {              /* end of line */
         break;
      }
      argv[argc++] = buf;              /* start argv[i] */
      *delim = '\0';                   /* terminate argv[i] */
      buf = delim + 1;                 /* start argv[i+1]? */
   }
   
   argv[argc] = NULL;

   if(argc == 0) {                     /* blank line */
      return 0;
   }

   // Should the job run in the background?
   if((bg =(strcmp(argv[argc-1], "&") == 0))) {
      argv[--argc] = NULL;
   }

   return bg;
}

/*----------------------------------------------------------------------------*/

// If first arg is a builtin command, run it and return true
int builtin(char *argv[]) {
   // Exit command
   if(strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
      if(verbose) {
         printf("%s %d: goodbye, world\n", prog, self_pid);
      }
      exit(0);
   } else if(strcmp(argv[0], "echo") == 0) {
      // Print (echo) each argument
      unsigned int i=1;
      while(argv[i] != NULL) {
         printf("%s ", argv[i]);
         i++;
      }
      // Print a trailing a newline
      printf("\n");
      return 0;
   }

   // ignore singleton &
   if(strcmp(argv[0], "&") == 0) {
      return 0;
   }

   // not a builtin command
   return 1;
}

/*----------------------------------------------------------------------------*/

void usage(int status) {
   if(status == EXIT_SUCCESS) {
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
