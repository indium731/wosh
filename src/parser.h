#ifndef WOSH_PARSER_H
#define WOSH_PARSER_H

#include "job.h"

char *wosh_read_line(void);
char **wosh_split_line(char *line);
char **wosh_parse_line(char **tokens);
process *wosh_create_processes(char **tokens);
job *wosh_create_job (process *processes);

#endif
