#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <linux/limits.h>

/*
  Function Declarations for builtin shell commands:
 */
int wosh_cd(char **args);
int wosh_help(char **args);
int wosh_exit(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit"
};

int (*builtin_func[]) (char **) = {
  &wosh_cd,
  &wosh_help,
  &wosh_exit
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
int wosh_cd(char **args)
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
int wosh_help(char **args)
{
  int i;
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < wosh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
   @return Always returns 0, to terminate execution.
 */
int wosh_exit(char **args)
{
  return 0;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */
int wosh_launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("wosh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("wosh");
  } else {
    // Parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int wosh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < wosh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return wosh_launch(args);
}

/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *wosh_read_line(void)
{
  char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us
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

/**
   @brief Loop getting input and executing it.
 */
void wosh_loop(void)
{
  char *line;
  char **args;
  int status;
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd() error");
    return;
  }

  do {
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      perror("getcwd() error");
      return;
    }
    char* dir = strcat(basename(cwd), " > ");
    printf(dir);
    line = wosh_read_line();
    args = wosh_split_line(line);
    status = wosh_execute(args);

    free(line);
    free(args);
  } while (status);
}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  // Load config files, if any.

  // Run command loop.
  wosh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}
