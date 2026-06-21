#ifndef WOSH_LAUNCH_H
#define WOSH_LAUNCH_H

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

#include "job.h"

void put_job_in_foreground (job *j, int cont);
void put_job_in_background (job *j, int cont);
void launch_process (process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground);
void continue_job (job *j, int foreground);
void launch_job (job *j, int foreground);


#endif
