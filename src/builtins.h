#ifndef WOSH_BUILTINS_H
#define WOSH_BUILTINS_H

int wosh_cd(char **args, int infile, int outfile, int errfile);
int wosh_help(char **args, int infile, int outfile, int errfile);
int wosh_exit(char **args, int infile, int outfile, int errfile);
int wosh_echo(char **args, int infile, int outfile, int errfile);
int wosh_num_builtins();

extern char *builtin_str[];

extern int (*builtin_func[]) (char **, int, int , int);

#endif
