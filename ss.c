/* SimpleShell, version 3
 *
 * Authors:    Shane Tully and Gage Ames
 */

#include "ss.h"

int main(int argc, char *argv[]) {
   char cmdline[MAX_LINE];
   int status = EXIT_SUCCESS;

   prog = argv[0];
   self_pid = (int) getpid();

   // Command line args parsing variables
   int ch;
   extern char *optarg;
   extern int optind;
   extern int optopt;
   extern int opterr;

   verbose = 0;
   debug = 0;
   echo = 0;
   exec = EXEC_VP;
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
   if ((ptable = allocate_process_table()) == NULL) {
      fprintf(stderr, "%s: Could not allocate the process table.", prog);
   }

   // Prevent SIGINT's from terminating the shell
   install_signal_handler(SIGINT, SIGINT_handler);

   if(verbose) {
      printf("%s %d: hello, world\n", prog, self_pid);
   }

   // Startup file
   FILE *startup;
   if(strcmp(startup_file, "-") == 0) {
      startup = stdin;
      startup_file = "[stdin]";
   } else {
      startup = fopen(startup_file, "r");
   }

   if(startup != NULL) {
      while(fgets(cmdline, MAX_LINE, startup) != NULL) {
         status = eval_line(cmdline);
      }

      if(ferror(startup) && custom_startup) {
         fprintf(stderr, "%s: error reading file %s: %s\n", prog, startup_file,
                 strerror(errno));
      }

      // Close the starup file (unless it is stdin)
      if(startup != stdin) {
         if(fclose(startup) != 0) {
            fprintf(stderr, "%s: cannot close file %s: %s\n", prog,
                    startup_file, strerror(errno));
         }
         startup = NULL;
      }
   } else if(custom_startup) {
      fprintf(stderr, "%s: cannot open startup file %s: %s\n", prog,
              startup_file, strerror(errno));
   }

   // Open input file
   FILE *infile;    
   char *infile_name;
   if(argv[optind] == NULL || strcmp(argv[optind], "-") == 0) {
      infile = stdin;
      infile_name = "[stdin]";
      
      // If stdin represents input from the terminal (as opposed to from a
      // pipe), make the shell interactive.
      if(isatty(fileno(stdin))) interactive = 1;
   } else {
      infile_name = argv[optind];  
      infile = fopen(infile_name, "r");
      if(infile == NULL) {
         fprintf(stderr, "%s: cannot open file %s: %s\n", prog, infile_name,
                 strerror(errno));
         Exit(EXIT_FAILURE);
      }
   }

   // If interactive, create the prompts
   if(interactive) {
      ps1 = get_prompt(0);
      ps2 = get_prompt(1);
   }

   // Main input loop
   char command[MAX_COMMAND];
   char *tmp_cmdline;
   int isLineCont = 0;
   while(1) {
      // If in interactive mode via stdin, show a prompt and get input from
      // readline
      if(interactive && infile == stdin) {         
         // Clear previous input from cmdline
         memset(cmdline, '\0', strlen(cmdline));

         // Read the input. Returns NULL on EOF
         if((tmp_cmdline = readline((isLineCont) ? ps2 : ps1)) == NULL) {
            printf("\n");
            Exit(0);
            continue;   // If exit returned there are background processes
         }

         // Copy tmp_cmdline into cmdline (check buffer sizes too)
         size_t tmp_cmdline_len = strlen(tmp_cmdline);
         if(tmp_cmdline_len >= MAX_LINE-2) {
            tmp_cmdline_len = MAX_LINE-2;
         }
         
         strncpy(cmdline, tmp_cmdline, tmp_cmdline_len);
         // eval_line expects all input to have a trailing newline
         cmdline[tmp_cmdline_len] = '\n';
         cmdline[tmp_cmdline_len+1] = '\0';

         // Add the input into the history
         add_history(tmp_cmdline);
      // If not interactive mode or reading from a file, use fgets to get input
      } else if(fgets(cmdline, MAX_LINE, infile) == NULL) {
         // Check if EOF was hit
         if (feof(infile)) {
            break;
         }

         // Check for error reading file
         if(ferror(infile)) {
            fprintf(stderr, "%s: error reading file %s: %s\n", prog,
                    infile_name, strerror(errno));
            break;
         }
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
   int background;                       /* should the job run in background or foreground? */
   int single = 1;                       /* there is only a single command in cmdline */
   pid_t pid;                            /* process id */
   int status = EXIT_SUCCESS;
   char *sc;                             /* index of semicolon in cmdline */
   size_t sc_len;                        /* length of sc */
   size_t cmdline_len = strlen(cmdline); /* length of cmdline */
   char buf[cmdline_len+1];              /* modified command */

   // Check for only single command
   if((sc = strchr(cmdline, ';')) != NULL) {
      single = 0;
   }

   // Check for semicolons denoting multiple commands in a single line of input
   while((sc = strchr(cmdline, ';')) != NULL || single) {
      // Only allow single to cause one loop iteration
      single = 0;

      if(sc != NULL) {
         // Copy cmdline up to the semicolon into buf
         sc_len = strlen(sc);
         strncpy(buf, cmdline, cmdline_len - sc_len);

         // The parse function expects whitespace at the end of each command
         // insert it and the null terminator
         buf[cmdline_len - sc_len] = '\n';
         buf[cmdline_len - sc_len+1] = '\0';

         // Move cmdline past the semicolon and update the len variable
         cmdline = sc+1;
         cmdline_len = strlen(cmdline);

         // Assume there's at least one more command after the current semi-colon
         // so set single back to 1 to force another loop iteration
         single = 1;
      } else {
         // If no semi-colon copy cmdline into buf verbatim
         strncpy(buf, cmdline, cmdline_len);
         buf[cmdline_len] = '\0';
      }

      if(debug) {
         fprintf(stderr, "%s: cmdline: %s\n", prog, cmdline);
      }

      // Build argv array
      background = parse(buf, argv);

      // Ignore empty lines
      if(argv[0] == NULL) {
         continue;
      }

      // If echo is on, print out the command
      if(echo) {
         printf("%s\n", buf);
      }

      // Check for builtin commands (exit, echo, etc.)
      if(builtin(argv) == 0) {
         continue;
      }

      // Create child to exexute command
      if((pid = fork()) == 0) {
         // Ignore SIGINT in the child -- our shell manages interrupts to children
         ignore_signal_handler(SIGINT);

         // Do any IO redirection if requested
         redirect(argv);

         // Try to replace the child with the desired program
         switch(exec) {
            // Hey! We can't convert from a array of unknown length at compile-time to 
            // a variable argument list so only send the 1st argument. This essentially
            // renders using execlp useless so it's more of a proof of concept than
            // anything else.
            case EXEC_LP:
               execlp(argv[0], argv[0]);
               break;
            case EXEC_VP:
               execvp(argv[0], argv);
               break;
            case EXEC_VE:
               execve(argv[0], argv, environ);
            default:;
         }
         execvp(argv[0], argv);
         printf("%s: failed: %s\n", buf, strerror(errno));
         _exit(EXIT_FAILURE);
      }

      // Parent waits for foreground job to terminate
      if(background) {
         if(verbose) {
            printf("background process %d: %s\n", (int) pid, buf);
         }
         insert_new_process(ptable, pid, buf);
      } else {
         foreground_pid = pid;
         if(waitpid(pid, &status, 0) == -1) {
            printf("%s: wait failed: %s\n", buf, strerror(errno));
            exit(EXIT_FAILURE);
         }
         foreground_pid = 0;

         if(verbose) {
            print_wait_status(pid, status);
         }
      }
   }

   return status;
}

/*----------------------------------------------------------------------------*/

// Parse the command line and build the argv array
int parse(char *buf, char *argv[]) {
   char *delim = NULL;   /* points to first whitespace delimiter */
   int argc = 0;         /* number of args */
   int bg;               /* background job? */
   int quote = 0;

   char whsp[] = " \t\n\v\f\r";          /* whitespace characters */

   /* Note - the trailing '\n' in buf is whitespace, and we need it as a delimiter. */

   // Treat anything following a '#' as a comment
   char *cmtIndex = strchr(buf, '#');
   if(cmtIndex != NULL) {
      if(debug) {
         fprintf(stderr, "%s: Ignoring comment: %s\n", prog, cmtIndex);
      }
      // Get a pointer to the end of the string
      char *end = strchr(cmtIndex, '\0');
      while(cmtIndex != end) end--;
      *end = '\0';
   }

   // Build the argv list
   while(1) {
      // Skip leading whitespace 
      buf += strspn(buf, whsp);

      // If buf starts a quote, skip to the end
      if(*buf == '\"') {
         quote = 1;
         // Skip the opening quote
         buf++;

         // Move delim to the closing quote
         // If a closing quote wasn't found, move buf back and treat the args as regular
         if((delim = strchr(buf, '\"')) == NULL) {
            buf--;
            quote = 0;
         } else {
            // Skip the closing quote, by replacing it with whitespace so it
            // isn't treated as an argument itself
            *delim = whsp[0];
         }
      }

      if(!quote) {
         // Skip to the next whitespace char or NULL
         delim = strpbrk(buf, whsp);
      }

      if(delim == NULL) {              /* end of line */
         break;
      }

      argv[argc++] = buf;              /* start argv[i] */
      *delim = '\0';                   /* terminate argv[i] */
      buf = delim + 1;                 /* start argv[i+1]? */

      // Reset quote
      quote = 0;
   }

   // NULL terminate the argv array
   argv[argc] = NULL;

   if(argc == 0) {                     /* blank line */
      return 0;
   }

   // If the last arg is an ampersand, the job should run in the background
   if((bg = (strcmp(argv[argc-1], "&") == 0))) {
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
      } else {
         Exit(0);
      }

      // Exit() will return if there are background jobs
      return 0;

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
      char *dir;
      if((dir = getcwd(NULL, MAX_PATH)) == NULL) {
         fprintf(stderr, "%s: Failed to get current working directory: %s\n", prog, strerror(errno));
         return 0;
      }
      printf("%s\n", dir);
      free(dir);
      dir = NULL;
      return 0;

   // cdir command
   } else if(strcmp(argv[0], "cdir") == 0 || strcmp(argv[0], "cd") == 0) {
      char *path;

      // If a path wasn't given, default to the HOME env
      if(argv[1] != NULL) {
         path = argv[1];
      } else if((path = getenv("HOME")) == NULL) {
         fprintf(stderr, "%s: Failed to change directory: %s\n", prog,
                 strerror(errno));
         return 0;
      }

      // Expand the given path to the full path
      char *fullPath = realpath(path, NULL);

      // Try to change the directory
      int dirChanged = 1;
      if(fullPath == NULL || chdir(fullPath) != 0) {
         fprintf(stderr, "%s: Failed to change directory to \"%s\": %s\n", prog,
                 path, strerror(errno));
         // The directory change failed
         dirChanged = 0;
      }

      if(dirChanged) {
         // Update PWD env
         if(setenv("PWD", fullPath, 1) != 0) {
            fprintf(stderr, "%s: Directory changed successfully, but failed to "
                    "update \'PWD\' enviroment variable: %s\n", prog,
                     strerror(errno));
         }
         // Update PS1 prompt
         free(ps1);
         ps1 = get_prompt(0);
      }

      free(fullPath);
      fullPath=NULL;

      return 0;

   // penv command
   } else if(strcmp(argv[0], "penv") == 0) {
      char *env;
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
            fprintf(stderr, "%s: Failed to set enviroment variable \"%s\" to \"%s\": %s\n", prog, argv[1], argv[2], strerror(errno));
         }
      } else {
         fprintf(stderr, "%s: Missing argument(s)\n", prog);
      }
      return 0;

   // unsenv command
   } else if(strcmp(argv[0], "unsenv") == 0) {
      if(argv[1] != NULL) {
         if(unsetenv(argv[1]) != 0) {
            fprintf(stderr, "%s: Failed to unset enviroment variable: \"%s\": %s\n", prog, argv[1], strerror(errno));
         }
      } else {
         fprintf(stderr, "%s: No argument specified\n", prog);
      }
      return 0;

   // pjobs command
   } else if(strcmp(argv[0], "pjobs") == 0) {
      // Print the process table of running jobs
      if(print_process_table(ptable) != 0) {
         fprintf(stderr, "%s: Failed to print process table\n", prog);
      }
      return 0;

   // limits command
   } else if(strcmp(argv[0], "limits") == 0) {
      // Print the constants used on input limits, # of child processes, etc
      printf("%s: Limits of the shell:\n", prog);
      printf("\tMax command length:\t%d chars\n", MAX_COMMAND);
      printf("\tMax line length:\t%d chars\n", MAX_LINE);
      printf("\tMax arguments:\t\t%d chars\n", MAX_ARGS);
      printf("\tMax path length:\t%d chars\n", MAX_PATH);
      printf("\tMax child processes:\t%d processes\n", MAX_CHILDREN);

      return 0;
   // set command
   } else if(strcmp(argv[0], "set") == 0) {
      // If no arguments, print all env's
      if(argv[1] == NULL) {
         char **env = environ;
         while(*env != NULL) {
            printf("%s\n", *env);
            env++;
        }
        return 0;
      }
      // All other commands require a second argument.
      // Make sure we have one
      if(argv[2] == NULL) {
         fprintf(stderr, "%s: Missing argument\n", prog);
         return 0;
      }

      if(strcmp(argv[1], "verbose") == 0) {
         if(strcmp(argv[2], "on") == 0) {
            verbose = 1;
         } else if(strcmp(argv[2], "off") == 0) {
            verbose = 0;
         } else {
            fprintf(stderr, "%s: Invalid argument: \"%s\"\n", prog, argv[2]);
         }
         return 0;
      } else if(strcmp(argv[1], "debug") == 0) {
         if(strcmp(argv[2], "on") == 0) {
            debug = 1;
            // Why not? Turn on echo and verbose as well
            echo = 1;
            verbose = 1;
         } else if(strcmp(argv[2], "off") == 0) {
            debug = 0;
            echo = 0;
            verbose = 0;
         } else {
            fprintf(stderr, "%s: Invalid argument: \"%s\"\n", prog, argv[2]);
         }
         return 0;
      } else if(strcmp(argv[1], "exec") == 0) {
         if(strcmp(argv[2], "lp") == 0) {
            exec = EXEC_LP;
         } else if(strcmp(argv[2], "vp") == 0) {
            exec = EXEC_VP;
         } else if(strcmp(argv[2], "ve") == 0) {
            exec = EXEC_VE;
         } else {
            fprintf(stderr, "%s: Invalid argument: \"%s\"\n", prog, argv[2]);
         }
         return 0;
      }
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
      printf("\tlimits\t\t\tPrint various limits of the shell\n");
      printf("\thelp\t\t\tPrint this message\n");
      printf("\tset\t\t\tPrint all enviroment variables\n");
      printf("\tset debug [on|off]\tEnable/disable debug output\n");
      printf("\tset exec [lp|vp|ve]\tSet the exec() function to use\n");
      printf("\tset verbose [on|off]\tEnable/disable verbose output\n");
      printf("\texit [n]\t\tExit the shell with specified exit code (0 if omitted)\n");
      printf("\tquit [n]\t\tSame as \'exit\'\n");
      return 0;

   // Ignore singleton '&'
   } else if (strcmp(argv[0], "&") == 0) {
      return 0;
   }

   // Not a builtin command
   return 1;
}

/*----------------------------------------------------------------------------*/

int redirect(char *argv[]) {
   // Redirect limitations:
   //    - All command arguments must come before redirection characters
   //    - "<<" is not supported
   //    - There must be a space before and after each redirection character

   FILE *new_stdin = NULL;
   FILE *new_stdout = NULL;
   int index = -1;
   int type = R_NONE;

   for(unsigned int i=1; argv[i] != NULL; i++) {
      if(strcmp(argv[i], "<") == 0 && argv[i+1] != NULL) {
         if(debug) {
            fprintf(stderr, "%s: Redirecting stdin to: \"%s\"\n", prog, argv[i+1]);
         }

         if((new_stdin = fopen(argv[i+1], "r")) == NULL) {
            fprintf(stderr, "%s: Failed to open input redirection file: \"%s\"\n", prog, argv[i+1]);
         }
         
         if(fclose(stdin) == EOF) {
            fprintf(stderr, "%s: Failed to close standard input stream for redirection\n", prog);
         }

         if(dup(fileno(new_stdin)) == -1) {
            fprintf(stderr, "%s: Failed to redirect standard input: %s\n", prog, strerror(errno));
         } else {
            // Update stdout to the new stdout FD
            stdin = new_stdin;
         }

         if(index == -1) index = i;

         type = (type == R_STDOUT) ? R_BOTH : R_STDIN;
      // Check if new_stdout is NULL to diallow both overwrite and append redirection
      } else if(strcmp(argv[i], ">") == 0 && argv[i+1] != NULL && new_stdout == NULL) {
         if(debug) {
            fprintf(stderr, "%s: Redirecting stdout (overwrite) to: \"%s\"\n", prog, argv[i+1]);
         }

         if((new_stdout = fopen(argv[i+1], "w")) == NULL) {
            fprintf(stderr, "%s: Failed to open output redirection file: \"%s\"\n", prog, argv[i+1]);
         }

         if(fclose(stdout) == EOF) {
            fprintf(stderr, "%s: Failed to close standard output stream for redirection\n", prog);
         }

         if(dup(fileno(new_stdout)) == -1) {
            fprintf(stderr, "%s: Failed to redirect standard input: %s\n", prog, strerror(errno));
         } else {
            // Update stdout to the new stdout FD
            stdout = new_stdout;
         }

         if(index == -1) index = i;

         type = (type == R_STDIN) ? R_BOTH : R_STDOUT;
      // Check if new_stdout is NULL to diallow both overwrite and append redirection
      } else if(strcmp(argv[i], ">>") == 0 && argv[i+1] != NULL && new_stdout == NULL) {
         if(debug) {
            fprintf(stderr, "%s: Redirecting stdout (append) to: \"%s\"\n", prog, argv[i+1]);
         }

         if((new_stdout = fopen(argv[i+1], "a")) == NULL) {
            fprintf(stderr, "%s: Failed to open output redirection file: \"%s\"\n", prog, argv[i+1]);
         }
         
         if(fclose(stdout) == EOF) {
            fprintf(stderr, "%s: Failed to close standard output stream for redirection\n", prog);
         }
         
         if(dup(fileno(new_stdout)) == -1) {
            fprintf(stderr, "%s: Failed to redirect standard input: %s\n", prog, strerror(errno));
         } else {
            // Update stdout to the new stdout FD
            stdout = new_stdout;
         }

         if(index == -1) index = i;

         type = (type == R_STDIN) ? R_BOTH : R_STDOUT;
      } else if(strlen(argv[i]) == 4 && argv[i][1] == '>' && argv[i][2] == '&') {
         // Check if redirecting stderr to stdout
         if(argv[i][0] == '2' && argv[i][3] == '1') {
            if(debug) {
               fprintf(stderr, "%s: Redirecting stderr to stdout\n", prog);
            }

            if(dup2(fileno(stdout), fileno(stderr)) == -1) {
               fprintf(stderr, "%s: Failed to redirect standard error to standard output: %s\n", prog, strerror(errno));
            }

            if(index == -1) index = i;
         // Check if redirecting stdout to stderr
         } else if(argv[i][0] == '1' && argv[i][3] == '2') {
            if(debug) {
               fprintf(stderr, "%s: Redirecting stdout to stderr\n", prog);
            }

            if(fclose(stdout) == EOF) {
               fprintf(stderr, "%s: Failed to close standard output stream for redirection\n", prog);
            }

            if(dup2(fileno(stdout), fileno(stderr)) == -1) {
               fprintf(stderr, "%s: Failed to redirect standard output to standard error: %s\n", prog, strerror(errno));
            }

            if(index == -1) index = i;
         }
      }
   }

   // Cutoff the argv array at the first IO redirection index
   if(index != -1) {
      argv[index] = NULL;
   }

   return type;
}

/*----------------------------------------------------------------------------*/

char* get_prompt(int isLineCont) {
   char user[MAX_LINE];
   char host[MAX_LINE];
   char *cwd = NULL;
   char *dir;
   char *prompt = malloc(MAX_LINE);

   // Make sure malloc didn't fail
   if(prompt == NULL) return NULL;

   if(!isLineCont) {
      // Show a prompt in the format of [user]@[hostname]$
      // If user or hostname cannot be determined, fallback to the process name
      // as the prompt
      if(getlogin_r(user, MAX_LINE) == 0 && gethostname(host, MAX_LINE) == 0 && (cwd = getcwd(NULL, MAX_PATH)) != NULL) {
         // Move cwd to the deepest directory
         dir = strrchr(cwd, '/')+1;
         sprintf(prompt, "%s@%s:%s$ ", user, host, dir);
      } else {
         sprintf(prompt, "%s$ ", prog);
      }

      free(cwd);
      cwd = NULL;
   // If line continuation, just show an arrow for the prompt
   } else {
      sprintf(prompt, "> ");
   }

   // Flush stdout to ensure the prompt gets written
   // (It doesn't end with a new line, so line buffering can delay its display)
   fflush(stdout);

   return prompt;
}

/*----------------------------------------------------------------------------*/

// Interrupt signal handler for the shell and foreground process
void SIGINT_handler(int sig) {
   if(sig != SIGINT) {
      fprintf(stderr, "%s: SIGINT_handler() recieved incorrect signal.", prog);
      return;
   }

   if(foreground_pid == 0) {
      if(verbose || debug) {
         fprintf(stderr, "SIGINT ignored");
      }
      printf("\n%s", ps1);
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

   if(ret == -1) {              // error condition
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
   // Check if any child processes have exited
   cleanup_terminated_children();

   if(ptable->children != 0) {
      printf("There %s %d background job%s running.\n",
             ((ptable->children > 1) ? "are" : "is"), ptable->children,
             ((ptable->children > 1) ? "s" : "" ));
      if(verbose) {
         if (print_process_table(ptable) != 0) {
            fprintf(stderr, "%s: Failed to print the process table.", prog);
         }
      }
   } else {
      deallocate_process_table(ptable);
      ptable = NULL;
      free(ps1);
      free(ps2);
      ps1 = NULL;
      ps2 = NULL;
      exit(status);
   }
}

/*----------------------------------------------------------------------------*/

void print_wait_status(pid_t pid, int status) {
   if(WIFEXITED(status)) {
      printf("process %d, completed normally, status %d\n", pid,
             WEXITSTATUS(status));
   } else {
      printf("process %d, completed abnormally, status %d\n", pid, status);
   }
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
