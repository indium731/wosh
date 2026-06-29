#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <termios.h>
#include <errno.h>

#include "job.h"
#include "launch.h"
#include "builtins.h"
#include "parser.h"

/**
   @brief Loop getting input and executing it.
 */
void wosh_loop(void)
{
  char *line;
	char **tokens;
  char **args;
  char cwd[PATH_MAX];
	job *curr_job;
	process *processes;

  do {
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      perror("getcwd() error");
      return;
    }
		printf("%s > ", basename(cwd));
		fflush(stdout);
    line = wosh_read_line();
    tokens = wosh_split_line(line);
		args = wosh_parse_line(tokens);
		processes = wosh_create_processes(args);
		curr_job = wosh_create_job(processes);
		launch_job(curr_job, 1);

    free(line);
		free(tokens);
    free(args);
  } while (1);
}



/* Make sure the shell is running interactively as the foreground job
   before proceeding. */

void
init_wosh()
{

  /* See if we are running interactively.  */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty (shell_terminal);

  if (shell_is_interactive)
    {
      /* Loop until we are in the foreground.  */
      while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
        kill (- shell_pgid, SIGTTIN);

      /* Ignore interactive and job-control signals.  */
      signal (SIGINT, SIG_IGN);
      signal (SIGQUIT, SIG_IGN);
      signal (SIGTSTP, SIG_IGN);
      signal (SIGTTIN, SIG_IGN);
      signal (SIGTTOU, SIG_IGN);
      signal (SIGCHLD, SIG_IGN);

      /* Put ourselves in our own process group.  */
      shell_pgid = getpid ();
      if (setpgid (shell_pgid, shell_pgid) < 0)
        {
          perror ("Couldn't put the shell in its own process group");
          exit (1);
        }

      /* Grab control of the terminal.  */
      tcsetpgrp (shell_terminal, shell_pgid);

      /* Save default terminal attributes for shell.  */
      tcgetattr (shell_terminal, &shell_tmodes);
    }
}
  

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  // run setup
  init_wosh();

  // Load config files, if any.

  // Run command loop.
  wosh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}
