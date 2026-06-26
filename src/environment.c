#include <stdlib.h>


/*
 * function prototypes
 */
char *wosh_path();
char *wosh_home();
char *wosh_user();

/*
  List of environment variable commands, followed by their corresponding functions.
 */
char *environvar_str[] = {
  "PATH",
  "HOME",
  "USER",
};

char *(*environvar_func[]) () = {
  &wosh_path,
  &wosh_home,
  &wosh_user,
};

int wosh_num_environvar() {
  return sizeof(environvar_str) / sizeof(char *);
}

/*
  environment variable function implementations.
*/

/**
 */
char *wosh_path()
{
	return getenv("PATH");
}

/**
 */
char *wosh_home()
{
	return getenv("HOME");
}

/**
 */
char *wosh_user()
{
	return getenv("USER");
}


