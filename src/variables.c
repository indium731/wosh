#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct variable {
    char *name;
    char *data;
} variable;

variable **var_str = NULL;

void init_vars(void)
{
    var_str = malloc(sizeof(*var_str));
    var_str[0] = NULL;
}

int get_var_str_size(void)
{
    int i;
    for (i = 0; var_str[i] != NULL; i++);
    return i;
}

void add_var(char *name, char *data)
{
    int size = get_var_str_size();

    var_str = realloc(var_str, (size + 2) * sizeof(*var_str));

    var_str[size] = malloc(sizeof(**var_str));
    var_str[size]->name = name;
    var_str[size]->data = data;
    var_str[size + 1] = NULL;
}

char *get_var(char *name)
{
	if (var_str == NULL) init_vars();
    for (int i = 0; var_str[i] != NULL; i++) {
        if (strcmp(var_str[i]->name, name) == 0)
            return var_str[i]->data;
    }
    return NULL;
}
