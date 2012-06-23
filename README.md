# SimpleShell

### Shane Tully and Gage Ames
### shanetully.com & gageames.com

SimpleShell is a very simple proof of concept shell. It was modeled after Bash and supports many of the same, basic features. Apart from the overall purpose of running commands, some of the features of the shell are below.

- Limits command:
   - The built in 'limits' command prints various constants in use by the shell.

- Runtime changing of verbose, debug, and exec:
   - The built in 'set' command allows for the changing of verbose, debug, and exec() type at runtime.
   - Arguments other than the program name are not passed when using execlp() due to it taking a variable argument list rather than an array as its arguments. There is an unknown number of arguments at compile time, thus it is not possible to know how many variable arguments to pass to execle() at runtime so we decided to only pass the first argument which is mandatory.

- Explicit environment variables passing:
   - If execve() is selected by use of the 'set' command described above, the enviroment variables are directly passed to the executed command.

- Comments:
   - Everything past and including a '#' character in a line will be ignored

- Line continuation:
   - Line continuation is supported by ending lines with a '\' character.

- IO redirection:
   - stdin redirection can be achieved with the '<' character
   - stdout overwrite redirection can be achieved with the '>' character
   - stdout append redirection can be acheived with the '>>' characters
   - stdin redirection with '<<' is not supported.
   - There must be at least one character of whitespace before and after any redirection character.
   - Non-redirection arguments after redirection arguments will be ignored.
   - Built in commands such as 'echo' do not respond to IO redirection.

- Error redirection:
   - stderr to stdout redirection can be acheived with '2>&1'
   - stdout to stderr redirection can be achieved with '1>&2'
   - If using IO redirection as documented above in conjunction with error redirection, error redirection must come after IO redirection.

- Multiple commands with ';':
   - Multiple commands can be entered on a single line if seperated with a semicolon.

- Command-line editing:
   - The implemented readline library allows for the interactive editing of command line input.

- Command history:
   - Previously entered commands can be recalled by using the up and down arrow keys.

- Filename and directory compeletion:
   - Pressing tab will try to fill in the name of a file or directory currently being entered.

- Quoted command-line arguments:
   - Arguments wrapped in double quotations are treated as a single argument.
   - Quoted arguments that include a semicolon are broken into two separate commands at the semicolon.

- Interactive mode detection:
   - By default, the shell starts in a non-interactive mode (it does not print a prompt). If the shell detects that stdin is connected to a terminal window (tty) then it will turn on interactive mode.
