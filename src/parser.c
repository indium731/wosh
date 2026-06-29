#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <termios.h>
#include <errno.h>
#include <stdbool.h>

#include "job.h"
#include "launch.h"
#include "builtins.h"
#include "variables.h"

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


    tokens[position] = token;
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
 * globbing helper functions
 *
 */ 

#define WOSH_STAR '*'

bool has_glob(const char *token)
{
	if (strchr(token, WOSH_STAR))
		return true;
	return false;
}

int is_dir(const char *path)
{
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}


int match_star(const char *pattern, const char *name)
{
  if (*pattern == '\0')
    return *name == '\0';

  if (*pattern == '*') {
    pattern++;

    if (*pattern == '\0')
      return 1;

    while (*name) {
      if (match_star(pattern, name))
        return 1;
      name++;
    }

  	return match_star(pattern, name);
  }

  if (*pattern == *name)
    return match_star(pattern + 1, name + 1);

  return 0;
}

void add_arg(char ***args, int *cap, const char *s)
{
	int count = 0;
	while(args[0][count] != NULL)
	{
		count++;
	}
  if (count + 1 >= *cap) {
    *cap *= 2;
    *args = realloc(*args, sizeof(char *) * (*cap));
    if (!*args) {
      perror("realloc");
      exit(1);
    }
	}

	args[0][count] = strdup(s);
  count++;
  args[0][count] = NULL;
}

void join_path(char *out, size_t size, const char *a, const char *b)
{
    if (strcmp(a, "") == 0)
        snprintf(out, size, "%s", b);
    else
        snprintf(out, size, "%s/%s", a, b);
}

void expand_glob_recursive(
  const char *base,
  char **parts,
  int index,
  int part_count,
  char ***results,
  int *result_cap
) {
  if (index == part_count) {
    add_arg(results, result_cap, base);
    return;
  }

  const char *pattern = parts[index];

  if (!has_glob(pattern)) {
    char next[1024];
    join_path(next, sizeof(next), base, pattern);

    if (index == part_count - 1 || is_dir(next)) {
      expand_glob_recursive(
        next,
        parts,
        index + 1,
        part_count,
        results,
        result_cap
      );
    }

    return;
  }

  const char *dir_to_open = strcmp(base, "") == 0 ? "." : base;

  DIR *dir = opendir(dir_to_open);
  if (!dir)
    return;

  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;

    if (name[0] == '.' && pattern[0] != '.')
      continue;

    if (!match_star(pattern, name))
      continue;

    char next[1024];
    join_path(next, sizeof(next), base, name);

    if (index == part_count - 1) {
      add_arg(results, result_cap, next);
    } else {
      if (is_dir(next)) {
        expand_glob_recursive(
          next,
          parts,
          index + 1,
          part_count,
          results,
          result_cap
        );
      }
    }
  }

  closedir(dir);
}

char **expand_one_glob(const char *token)
{
  char *copy = strdup(token);

  int cap = 8;
  int count = 0;
  char **parts = malloc(sizeof(char *) * cap);

  char *part = strtok(copy, "/");

  while (part) {
    if (count >= cap) {
      cap *= 2;
      parts = realloc(parts, sizeof(char *) * cap);
    }

    parts[count++] = part;
    part = strtok(NULL, "/");
  }

  int result_cap = 8;
  char **results = malloc(sizeof(char *) * result_cap);
  results[0] = NULL;

  expand_glob_recursive(
    "",
    parts,
    0,
    count,
    &results,
    &result_cap
  );

  free(parts);
  free(copy);

  return results;
}
/*
 *
 * expand the input with globbing and the like
 *
 */ 


char **wosh_parse_line(char **tokens)
{
  int cap = 16;
  char **argv = malloc(sizeof(char *) * cap);
  argv[0] = NULL;

  for (int i = 0; tokens[i] != NULL; i++) {

		if (tokens[i][0] == '~')
		{
			tokens[i] = getenv("HOME");
		}else if (tokens[i][0] == '$')
		{
			if (getenv(&tokens[i][1]) != NULL)
			{
				tokens[i] = getenv(&tokens[i][1]);
			}else if (get_var(&tokens[i][1]) != NULL)
				tokens[i] = get_var(&tokens[i][1]);
		}

  	if (has_glob(tokens[i])) {
    	int glob_count = 0;
    	char **matches = expand_one_glob(tokens[i]);

			while (matches[glob_count] != NULL){
				glob_count++;
			}

	    if (glob_count == 0) {
	      add_arg(&argv, &cap, tokens[i]);
	    } else {
	      for (int j = 0; j < glob_count; j++) {
	        add_arg(&argv, &cap, matches[j]);
	        free(matches[j]);
	      }
	    }

	    free(matches);
	  } else {
      add_arg(&argv, &cap, tokens[i]);
    }
	}
  return argv;
}


/*
 *
 * create a list of processes based on tokenized input
 *
 */

#define WOSH_PIPE '|'
#define WOSH_REDIRECTION '>'

process *wosh_create_processes(char **tokens)
{
	char *curr_pointer = tokens[0];
	int args_counter = 0;
	int process_counter = 1;
	process *processes;
	processes = (process*)malloc(sizeof(process));
	processes[0].argv = NULL;
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
			processes[process_counter-1].infile = NULL;
			processes[process_counter-1].outfile = NULL;
			processes[process_counter-1].errorfile = NULL;
			tokens = &tokens[args_counter+1];
			process_counter++;
			args_counter = 0;

		} else if (*curr_pointer == WOSH_REDIRECTION)
			{
    		if (tokens[args_counter + 1] == NULL) 
				{
        	fprintf(stderr, "syntax error: expected filename after >\n");
        	return NULL;
    		}

    		processes[process_counter - 1].outfile = tokens[args_counter + 1];

    		tokens[args_counter] = NULL;

    		// after filename, only NULL or pipe should be allowed
    		if (tokens[args_counter + 2] != NULL &&
        	tokens[args_counter + 2][0] != WOSH_PIPE) 
				{
        	fprintf(stderr, "syntax error: unexpected token after redirection filename: %s\n",
                tokens[args_counter + 2]);
        	return NULL;
    		}
    		args_counter += 2;
			}
		args_counter++;
		curr_pointer = tokens[args_counter];
	}
	if (!processes[process_counter-1].argv)
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

