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


#include "launch.h"

/*
  Function Declarations for builtin shell commands:
 */
int wosh_cd(char **args, int infile, int outfile, int errfile);
int wosh_help(char **args, int infile, int outfile, int errfile);
int wosh_exit(char **args, int infile, int outfile, int errfile);
int wosh_echo(char **args, int infile, int outfile, int errfile);
int wosh_fg (char **args, int infile, int outfile, int errfile);
int wosh_bg (char **args, int infile, int outfile, int errfile);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
	"echo",
	"fg",
	"bg"
};

int (*builtin_func[]) (char **, int, int , int) = {
  &wosh_cd,
  &wosh_help,
  &wosh_exit,
	&wosh_echo,
	&wosh_fg,
	&wosh_bg
};

int wosh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

int is_builtin (char *argv)
{
	for (int i = 0; i < wosh_num_builtins(); i++) 
	{
	if (strcmp(argv, builtin_str[i]) == 0) 
		return 1;
	}
	return 0;
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
  return 0;
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
  return 0;
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

	return 0;
}

int wosh_fg (char **args, int infile, int outfile, int errfile)
{
	if (!args[1])
	{
		job *j = find_last_job ();
		if (!j)
			return 1;
		continue_job (j, 1);
		return 0;
	}
	return 1;
	//fg args not implemented
}

int wosh_bg (char **args, int infile, int outfile, int errfile)
{
	if (!args[1])
	{
		put_job_in_background (find_last_job (), 0);
		return 0;
	}
	return 1;
	//fg args not implemented
}
