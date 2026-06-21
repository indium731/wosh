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

/*
 * Data structures for the shell
 */

typedef struct process
{
  struct process *next;       /* next process in pipeline */
  char **argv;                /* for exec */
  pid_t pid;                  /* process ID */
  char completed;             /* true if process has completed */
  char stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
} process;

typedef struct job
{
  struct job *next;           /* next active job */
  char *command;              /* command line, used for messages */
  process *first_process;     /* list of processes in this job */
  pid_t pgid;                 /* process group ID */
  char notified;              /* true if user told about stopped job */
  struct termios tmodes;      /* saved terminal modes */
  int standardin, standardout, standarderror;  /* standard i/o channels */
} job;

job *first_job = NULL;

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

/*
 * Function prototypes
 */

void put_job_in_foreground (job *j, int cont);
void put_job_in_background (job *j, int cont);
void wait_for_job (job* j);

/*
  Function Declarations for builtin shell commands:
 */
int wosh_cd(char **args, int infile, int outfile, int errfile);
int wosh_help(char **args, int infile, int outfile, int errfile);
int wosh_exit(char **args, int infile, int outfile, int errfile);
int wosh_echo(char **args, int infile, int outfile, int errfile);


/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
	"echo"
};

int (*builtin_func[]) (char **, int, int , int) = {
  &wosh_cd,
  &wosh_help,
  &wosh_exit,
	&wosh_echo
};

int wosh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/

/**
   @brief Builtin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int wosh_cd(char **args, int infile, int outfile, int errfile)
{
  if (args[1] == NULL) {
    fprintf(stderr, "wosh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("wosh");
    }
  }
  return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args.  Not examined.
   @return Always returns 1, to continue executing.
 */
int wosh_help(char **args, int infile, int outfile, int errfile)
{
  int i;
  fprintf(stdout, "Type program names and arguments, and hit enter.\n");
  fprintf(stdout, "The following are built in:\n");

  for (i = 0; i < wosh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  fprintf(stdout, "Use the man command for information on other programs.\n");
  return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
 */
int wosh_exit(char **args, int infile, int outfile, int errfile)
{
	exit (0);
}


/**
   @brief Builtin command: echo.
   @param args List of args.  Not examined.
 */
int wosh_echo (char **args, int infile, int outfile, int errfile)
{
	int i = 1;
	while (args[i])
	{
		write(outfile, args[i], strlen(args[i]));
		if (args[i+1])
			write(outfile, " ", 1);
		i++;
	}
	write(outfile, "\n", 1);

	return 1;
}


/* 
 * utility functions for operating on job objects
 */

job *
find_job (pid_t pgid)
{
  job *j;

  for (j = first_job; j; j = j->next)
    if (j->pgid == pgid)
      return j;
  return NULL;
}


int
job_is_stopped (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed && !p->stopped)
      return 0;
  return 1;
}

int
job_is_completed (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed)
      return 0;
  return 1;
}

void
put_job_in_foreground (job *j, int cont)
{
  /* Put the job into the foreground.  */
  tcsetpgrp (shell_terminal, j->pgid);


  /* Send the job a continue signal, if necessary.  */
  if (cont)
    {
      tcsetattr (shell_terminal, TCSADRAIN, &j->tmodes);
      if (kill (- j->pgid, SIGCONT) < 0)
        perror ("kill (SIGCONT)");
    }


  /* Wait for it to report.  */
  wait_for_job (j);

  /* Put the shell back in the foreground.  */
  tcsetpgrp (shell_terminal, shell_pgid);

  /* Restore the shell’s terminal modes.  */
  tcgetattr (shell_terminal, &j->tmodes);
  tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
}

void
put_job_in_background (job *j, int cont)
{
  /* Send the job a continue signal, if necessary.  */
  if (cont)
    if (kill (-j->pgid, SIGCONT) < 0)
      perror ("kill (SIGCONT)");
}


void 
launch_process (process *p, pid_t pgid,
                int infile, int outfile, int errfile,
                int foreground)
{
  pid_t pid;

  if (shell_is_interactive)
    {
      /* Put the process into the process group and give the process group
         the terminal, if appropriate.
         This has to be done both by the shell and in the individual
         child processes because of potential race conditions.  */
      pid = getpid ();
      if (pgid == 0) pgid = pid;
      setpgid (pid, pgid);
      if (foreground)
        tcsetpgrp (shell_terminal, pgid);

      /* Set the handling for job control signals back to the default.  */
      signal (SIGINT, SIG_DFL);
      signal (SIGQUIT, SIG_DFL);
      signal (SIGTSTP, SIG_DFL);
      signal (SIGTTIN, SIG_DFL);
      signal (SIGTTOU, SIG_DFL);
      signal (SIGCHLD, SIG_DFL);
    }

  /* Set the standard input/output channels of the new process.  */
  if (infile != STDIN_FILENO)
    {
      dup2 (infile, STDIN_FILENO);
      close (infile);
    }
  if (outfile != STDOUT_FILENO)
    {
      dup2 (outfile, STDOUT_FILENO);
      close (outfile);
    }
  if (errfile != STDERR_FILENO)
    {
      dup2 (errfile, STDERR_FILENO);
      close (errfile);
    }

	//check if the process to be launched is part of the builtins
	//if so just return since this is run from a fork and so the parent process should run the builtin
  for (int i = 0; i < wosh_num_builtins(); i++) {
    if (strcmp(p->argv[0], builtin_str[i]) == 0) {
			exit (1);
    }
  }

  /* Exec the new process.  Make sure we exit.  */
  execvp (p->argv[0], p->argv);
  perror ("execvp");
  exit (1);
}

void
launch_job (job *j, int foreground)
{
  process *p;
  pid_t pid;
  int mypipe[2], infile, outfile;

  infile = j->standardin;
  for (p = j->first_process; p; p = p->next)
    {
      /* Set up pipes, if necessary.  */
      if (p->next)
        {
          if (pipe (mypipe) < 0)
            {
              perror ("pipe");
              exit (1);
            }
          outfile = mypipe[1];
        }
      else
        outfile = j->standardout;

			//check if the process to be launched is part of the builtins
			for (int i = 0; i < wosh_num_builtins(); i++) {
				if (strcmp(p->argv[0], builtin_str[i]) == 0) {
					(*builtin_func[i])(p->argv, infile, outfile, j->standarderror);
					continue;
				}
			}
      /* Fork the child processes.  */
      pid = fork ();
      if (pid == 0)
        /* This is the child process.  */
        launch_process (p, j->pgid, infile,
                        outfile, j->standarderror, foreground);
      else if (pid < 0)
        {
          /* The fork failed.  */
          perror ("fork");
          exit (1);
        }
      else
        {
          /* This is the parent process.  */
          p->pid = pid;
          if (shell_is_interactive)
            {
              if (!j->pgid)
                j->pgid = pid;
              setpgid (pid, j->pgid);
            }
        }

      /* Clean up after pipes.  */
      if (infile != j->standardin)
        close (infile);
      if (outfile != j->standardout)
        close (outfile);
      infile = mypipe[0];
    }


  if (!shell_is_interactive)
    wait_for_job (j);
  else if (foreground)
    put_job_in_foreground (j, 0);
  else
    put_job_in_background (j, 0);
}


int
mark_process_status (pid_t pid, int status)
{
  job *j;
  process *p;


  if (pid > 0)
    {
      /* Update the record for the process.  */
      for (j = first_job; j; j = j->next)
        for (p = j->first_process; p; p = p->next)
          if (p->pid == pid)
            {
              p->status = status;
              if (WIFSTOPPED (status))
                p->stopped = 1;
              else
                {
                  p->completed = 1;
                  if (WIFSIGNALED (status))
                    fprintf (stderr, "%d: Terminated by signal %d.\n",
                             (int) pid, WTERMSIG (p->status));
                }
              return 0;
             }
      fprintf (stderr, "No child process %d.\n", pid);
      return -1;
    }

  else if (pid == 0 || errno == ECHILD)
    /* No processes ready to report.  */
    return -1;
  else {
    /* Other weird errors.  */
    perror ("waitpid");
    return -1;
  }
}


/* Check for processes that have status information available,
   without blocking.  */

void
update_status (void)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (-1, &status, WUNTRACED|WNOHANG);
  while (!mark_process_status (pid, status));
}


void
wait_for_job (job *j)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (-1, &status, WUNTRACED);
  while (!mark_process_status (pid, status)
         && !job_is_stopped (j)
         && !job_is_completed (j));
}



/* Notify the user about stopped or terminated jobs.
   Delete terminated jobs from the active job list.  */

void
do_job_notification (void)
{
  job *j, *jlast, *jnext;

  /* Update status information for child processes.  */
  update_status ();

  jlast = NULL;
  for (j = first_job; j; j = jnext)
    {
      jnext = j->next;

      /* If all processes have completed, tell the user the job has
         completed and delete it from the list of active jobs.  */
      if (job_is_completed (j)) {
        if (jlast)
          jlast->next = jnext;
        else
          first_job = jnext;
        free (j);
      }

      /* Notify the user about stopped jobs,
         marking them so that we won’t do this more than once.  */
      else if (job_is_stopped (j) && !j->notified) {
        j->notified = 1;
        jlast = j;
      }

      /* Don’t say anything about jobs that are still running.  */
      else
        jlast = j;
    }
}void
mark_job_as_running (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    p->stopped = 0;
  j->notified = 0;
}


/* Continue the job J.  */

void
continue_job (job *j, int foreground)
{
  mark_job_as_running (j);
  if (foreground)
    put_job_in_foreground (j, 1);
  else
    put_job_in_background (j, 1);
}






/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *wosh_read_line(void)
{
  char *line = NULL;
  size_t bufsize = 0; // have getline allocate a buffer for us
  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin)) {
      exit(EXIT_SUCCESS);  // We received an EOF
    } else  {
      perror("wosh: getline\n");
      exit(EXIT_FAILURE);
    }
  }
  return line;
}

#define WOSH_TOK_BUFSIZE 64
#define WOSH_TOK_DELIM " \t\r\n\a"
#define WOSH_TOK_DELIM_FULL " \t\r\n\a\'"
/**
   @brief Split a line into tokens (very naively).
   @param line The line.
   @return Null-terminated array of tokens.
 */

char **wosh_split_line(char *line)
{
  int bufsize = WOSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;
  char *line_start;

  if (!tokens) {
    fprintf(stderr, "wosh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  line_start = strdup(line);

  token = strtok(line, WOSH_TOK_DELIM);
  line_start = &line_start[strcspn(line_start, WOSH_TOK_DELIM)]+1;
  while (token != NULL) {

		if (token[0] == '~')
		{
			tokens[position] = getenv("HOME");
		}else{
    	tokens[position] = token;
		}
    position++;

    if (position >= bufsize) {
      bufsize += WOSH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
				free(tokens_backup);
        exit(EXIT_FAILURE);
      }
    }


    if (line_start[0] == '\''){
	    token = strtok(&line_start[1], "\'");
	    line_start = &line_start[strcspn(&line_start[1], "\'")]+1;
    }else{
    	token = strtok(NULL, WOSH_TOK_DELIM);
			line_start = &line_start[strcspn(line_start, WOSH_TOK_DELIM)]+1;
    }
  }
  tokens[position] = NULL;
  return tokens;
}

/*
 *
 * create a list of processes based on tokenized input
 *
 */

#define WOSH_PIPE '|'

process *wosh_create_processes(char **tokens)
{
	char *curr_pointer = tokens[0];
	int args_counter = 0;
	int process_counter = 1;
	process *processes;
	processes = (process*)malloc(sizeof(process));
	process *processes_backup;

	if (!processes)
	{
		fprintf(stderr, "wosh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (curr_pointer)
	{
		if (*curr_pointer == WOSH_PIPE)
		{

			processes_backup = processes;
			processes = (process*)realloc(processes, (process_counter+1) * sizeof(process));

			if (!processes)
			{
				free(processes_backup);
				exit(EXIT_FAILURE);
			}

			tokens[args_counter] = NULL;
			processes[process_counter-1].argv = tokens;
			tokens = &tokens[args_counter+1];
			process_counter++;
			args_counter = 0;

		}
		args_counter++;
		curr_pointer = tokens[args_counter];
	}
	processes[process_counter-1].argv = tokens;

	for (int i = 0;i < process_counter -1; i++)
	{
		processes[i].next = &processes[i+1];
	}

	processes[process_counter - 1].next = NULL;

	return processes;
}



/*
 *
 * create a job based on processes
 *
 */

job *wosh_create_job (process *processes)
{

	job *new_job;

	new_job = (job*)malloc(sizeof(job));

	if (!new_job)
	{
		fprintf(stderr, "wosh: allocation error");
		exit (EXIT_FAILURE);
	}


	new_job->first_process = processes;
	new_job->standardin = 0;
	new_job->standardout = 1;
	new_job->standarderror = 2;


	return new_job;

}

/**
   @brief Loop getting input and executing it.
 */
void wosh_loop(void)
{
  char *line;
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
    args = wosh_split_line(line);
		processes = wosh_create_processes(args);
		curr_job = wosh_create_job(processes);
		launch_job(curr_job, 1);

    free(line);
    free(args);
  } while (1);
}

/* 
 * setup esc sequences
 */

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;


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
