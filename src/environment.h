#ifndef WOSH_ENVIRONMENT_H
#define WOSH_ENVIRONMENT_H

char *wosh_path();
char *wosh_home();
char *wosh_user();
int wosh_num_environvar();

extern char *environvar_str[];

extern char *(*environvar_func[]) ();

#endif
