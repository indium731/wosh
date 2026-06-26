#ifndef WOSH_ENVIRONMENT_H
#define WOSH_ENVIRONMENT_H

#include <stdlib.h>
#include <stdio.h>

typedef struct variable
{
	char *name;
	char *data
} variable;

int get_var_str_size();

void add_var(char *name, char *data);
char *get_var(char *name);

#endif
