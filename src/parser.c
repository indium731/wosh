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
#include "environment.h"

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
		}else if (token[0] == '$'){
			//check if its a environment variable
			for (int i = 0; i < wosh_num_environvar(); i++)
			{
				if (strcmp(&token[1], environvar_str[i]) == 0)
				{
					tokens[position] = (*environvar_func[i])();
				}
			}
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

