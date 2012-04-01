/* CMPSC 311 Project 7, version 3
 *
 * Authors:    Shane Tully and Gage Ames
 * Emails:     shanet@psu.edu and gra5028@psu.edu
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
   int interactive = 0;
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

   // Prevent Ctrl+C from terminating the shell
   install_signal_handler(SIGINT, SIGINT_handler);

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
         Exit(EXIT_FAILURE);
      }
   }

   // Main input loop
   char command[COMMAND_LEN];
   int isLineCont = 0;
   while(1) {
      // If interctive mode, show a prompt
      if(interactive) {
         print_prompt(isLineCont);
      }

      // Read command from input
      // cmdline includes trailing newline
      fgets(cmdline, MAX_LINE, infile);

      if(ferror(infile)) {
         fprintf(stderr, "%s: error reading file %s: %s\n", prog, infile_name,
                 strerror(errno));
         break;
      }

      // End of input
      if(feof(infile)) {
         break;
      }

      // Check for line continuation
      char *end = strchr(cmdline, '\0');
      // Subtract the null-terminator and newline
      end-=2;
      if(*end == '\\') {
         // Remove the \ char
         *end = '\0';
         strncat(command, cmdline, strlen(cmdline));
         isLineCont = 1;
         continue;
      } else if(isLineCont) {
         // This is end line of a line continuation so append it to the command buffer,
         // and then allow it to be eval'd below
         strncat(command, cmdline, strlen(cmdline));
      } else {
         // Normal line. Overwrite the commnd buffer.
         strncpy(command, cmdline, strlen(cmdline));
      }

      cleanup_terminated_children();

      // Check that the line about the parsed doesn't end with a line continuation char
      if(*(strchr(cmdline, '\0')-2) == '\\') {
         fprintf(stderr, "%s: Input with line continuation at end of input\n", prog);
         break;
      }

      // Parse the line
      status = eval_line(command);

      // Reset command buffer and line continuation flag for the next line of input
      isLineCont = 0;
      memset(command, '\0', strlen(command));
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

// Evaluate a command line
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
      // Ignore SIGINT in the child -- our shell manages interrupts to children
      ignore_signal_handler(SIGINT);

      // Try to replace the child with the desired program 
      if(execvp(argv[0], argv) == -1) {
         printf("%s: failed: %s\n", argv[0], strerror(errno));
         _exit(EXIT_FAILURE);
      }
   }

   // Parent waits for foreground job to terminate
   if(background) {
      printf("background process %d: %s", (int) pid, cmdline);
      insert_new_process(ptable, pid, argv[0]);
   } else {
      foreground_pid = pid;
      if(waitpid(pid, &status, 0) == -1) {
         printf("%s: failed: %s\n", argv[0], strerror(errno));
         exit(EXIT_FAILURE);
      }
      foreground_pid = 0;

      if (verbose) {
          print_wait_status(pid, status);
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

   // Treat anything following a '#' as a comment
   char *cmtIndex = strchr(buf, '#');
   if(cmtIndex != NULL) {
      // Get a pointer to the end of the string
      char *end = strchr(cmtIndex, '\0');
      while(cmtIndex != end) end--;
      *end = '\0';
   }

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
   // exit/quit command
   if(strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
      if(verbose) {
         printf("%s (%d): goodbye, world\n", prog, self_pid);
      }

      // Check if an exit code was supplied
      int exitCode;
      if(argv[1] != NULL && (exitCode = atoi(argv[1])) != 0) {
         Exit(exitCode);
      }

      Exit(0);
   // echo command
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
   // dir command
   } else if(strcmp(argv[0], "dir") == 0) {
      char *dir = getcwd(NULL, _MAX_INPUT);
      printf("%s\n", dir);
      free(dir);
      dir = NULL;
      return 0;
   // cdir command
   } else if(strcmp(argv[0], "cdir") == 0) {
      char *path;
      // If a path wasn't given, default to the HOME env
      if(argv[1] != NULL) {
         path = argv[1];
      } else if((path = getenv("HOME")) == NULL) {
         fprintf(stderr, "%s: Failed to change directory\n", prog);
         return 0;
      }

      // Try to change the directory
      if(chdir(path) != 0) {
         fprintf(stderr, "%s: Failed to change directory to \"%s\"\n", prog, path);
         return 0;
      }

      // Update PWD env
      char fullPath[PATH_MAX];
      if(realpath(path, fullPath) == NULL || setenv("PWD", fullPath, 1) != 0) {
         fprintf(stderr, "%s: Directory changed successfully, but failed to update \"PWD\" enviroment variable\n", prog);
      }

      return 0;
   // penv command
   } else if(strcmp(argv[0], "penv") == 0) {
      char *env;
      extern char **environ;
      // If there aren't any arguments, print all envs
      if(argv[1] == NULL) {
         unsigned int i=0;
         while(environ[i] != NULL) {
            printf("%s\n", environ[i]);
            i++;
         }
      // Print the specified env
      } else {
         if((env = getenv(argv[1])) != NULL) {
            printf("%s=%s\n", argv[1], env);
         }
      }
      return 0;
   // senv command
   } else if(strcmp(argv[0], "senv") == 0) {
      if(argv[1] != NULL && argv[2] != NULL) {
         if(setenv(argv[1], argv[2], 1) != 0) {
            fprintf(stderr, "%s: Failed to set enviroment variable \"%s\" to \"%s\"\n", prog, argv[1], argv[2]);
         }
      } else {
         fprintf(stderr, "%s: Missing argument(s)\n", prog);
      }
      return 0;
   // unsenv command
   } else if(strcmp(argv[0], "unsenv") == 0) {
      if(argv[1] != NULL) {
         if(unsetenv(argv[1]) != 0) {
            fprintf(stderr, "%s: Failed to unset enviroment variable: \"%s\"\n", prog, argv[1]);
         }
      } else {
         fprintf(stderr, "%s: No argument specified\n", prog);
      }
      return 0;
   // pjobs commands
   } else if (strcmp(argv[0], "pjobs") == 0) {
      // Print the process table of running jobs
      print_process_table(ptable);
      return 0;
   // help command
   } else if(strcmp(argv[0], "help") == 0) {
      printf("%s: Built-in commands:\n", prog);
      printf("\techo [args]\t\tPrint any arguments given\n");
      printf("\tdir\t\t\tPrint the current working directory\n");
      printf("\tcdir [directory]\tChange the current working directory\n");
      printf("\tpenv [env]\t\tPrint specified enviroment variable or none for all variables\n");
      printf("\tsenv [env] [value]\tSet the specified enviroment variable to the specified value\n");
      printf("\tunsenv [env]\t\tUnset the specified enviroment variable\n");
      printf("\tpjobs\t\t\tPrint table of all running jobs\n");
      printf("\thelp\t\t\tPrint this message\n");
      printf("\texit [n]\t\t\tExit the shell with specified exit code (0 if omitted)\n");
      printf("\tquit [n]\t\t\tSame as \'exit\'\n");
      return 0;
   // Ignore singleton '&'
   } else if (strcmp(argv[0], "&") == 0) {
      return 0;
   }

   // Not a builtin command
   return 1;
}

/*----------------------------------------------------------------------------*/

void print_prompt(int isLineCont) {
   if(!isLineCont) {
      // Show a prompt in the format of [user]@[hostname]$
      // If user or hostname cannot be determined, fallback to the process name
      // as the prompt
      char user[_MAX_INPUT];
      char host[_MAX_INPUT];
      if(getlogin_r(user, _MAX_INPUT) == 0 && gethostname(host, _MAX_INPUT) == 0) {
         printf("%s@%s$ ", user, host);
      } else {
         printf("%s$ ", prog);
      }
   // If line continuation, just show an arrow for the prompt
   } else {
      printf("> ");
   }

   // Flush stdout to ensure the prompt gets written
   // (It doesn't end with a new line, so line buffering can delay its display)
   fflush(stdout);
}

/*----------------------------------------------------------------------------*/

// Interrupt signal handler for the shell and foreground process
void SIGINT_handler(int sig) {
   if (foreground_pid == 0) {
      fprintf(stderr, "SIGINT ignored\n");
      print_prompt(0);
   } else {
      kill(foreground_pid, SIGINT);
      foreground_pid = 0;
   }
}

// Install signal handler using sigaction
// return 0 if successful, -1 if not
int install_signal_handler(int sig, sighandler_t func) {
   struct sigaction sigact;		// signal handler descriptor
   int ret;				            // error indicator

   sigact.sa_handler = func;     // name of the signal handler function
   sigemptyset(&sigact.sa_mask); // do not mask any interrupts while in handler
   sigact.sa_flags = SA_RESTART; // restart system functions if interrupted

   ret = sigaction(sig, &sigact, NULL);	// do the installation

   if (ret == -1) {              // error condition
      fprintf(stderr, "install_signal_handler(%d) failed: %s\n", sig,
              strerror(errno));
   }

   return ((ret == -1) ? -1 : 0);
}

// Ignore a signal handler using sigaction
// Used for background processes
// return 0 if successful, -1 if not */
int ignore_signal_handler(int sig) {
   struct sigaction sigact;      // signal handler descriptor
   int ret;                      // error indicator

   sigact.sa_handler = SIG_IGN;  // ignore the signal
   sigemptyset(&sigact.sa_mask); // do not mask any interrupts while in handler
   sigact.sa_flags = SA_RESTART; // restart system functions if interrupted

   ret = sigaction(sig, &sigact, NULL);   // do the installation

   if (ret == -1) {              // error condition
      fprintf(stderr, "ignore_signal_handler(%d) failed: %s\n", sig,
              strerror(errno));
   }

   return ((ret == -1) ? -1 : 0);
}

/*----------------------------------------------------------------------------*/

// Ensure no background processes are running and deallocate memory before
// exiting
void Exit(int status) {
   if (ptable->children != 0) {
      printf("There %s %d background job%s running.\n",
             ((ptable->children > 1) ? "are" : "is"), ptable->children,
             ((ptable->children > 1) ? "s" : "" ));
      if (verbose) {
         print_process_table(ptable);
      }
   } else {
      deallocate_process_table(ptable);
      exit(status);
   }
}

/*----------------------------------------------------------------------------*/

void print_wait_status(pid_t pid, int status) {
   printf("process %d, completed %snormally, status %d\n", pid,
          ((status >= 0) ? "" : "ab"), status);
}

/*----------------------------------------------------------------------------*/

//Find all the child processes that have terminated, without waiting.
void cleanup_terminated_children(void) {
   pid_t pid;
   int status;

   while (1)
   {
      pid = waitpid(-1, &status, WNOHANG);

      if (pid == 0) {           /* returns 0 if no child process to wait for */
         break;
      }

      if (pid == -1) {          /* returns -1 if there was an error */
         /* errno will have been set by waitpid() */
         if (errno == ECHILD) { /* no children */
            break;
         }
         if (errno == EINTR) {  /* waitpid() was interrupted by a signal */
            continue;           /* try again */
         } else {
            printf("unexpected error in cleanup_terminated_children(): %s\n",
                   strerror(errno));
            break;
         }
      }

      print_wait_status(pid, status);
      update_existing_process(ptable, pid, status);
      remove_old_process(ptable, pid);
   }

  return;
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
