#ifndef WOSH_JOB_H
#define WOSH_JOB_H

#include <sys/types.h>
#include <termios.h>

typedef struct process
{
  struct process *next;       /* next process in pipeline */
  char **argv;                /* for exec */
  pid_t pid;                  /* process ID */
  char completed;             /* true if process has completed */
  char stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
} process;

typedef struct job
{
  struct job *next;           /* next active job */
  char *command;              /* command line, used for messages */
  process *first_process;     /* list of processes in this job */
  pid_t pgid;                 /* process group ID */
  char notified;              /* true if user told about stopped job */
  struct termios tmodes;      /* saved terminal modes */
  int standardin, standardout, standarderror;  /* standard i/o channels */
} job;

extern job *first_job;

extern pid_t shell_pgid;
extern struct termios shell_tmodes;
extern int shell_terminal;
extern int shell_is_interactive;

void wait_for_job (job* j);
job * find_job (pid_t pgid);
int job_is_stopped (job *j);
int job_is_completed (job *j);
int mark_process_status (pid_t pid, int status);
void update_status (void);
void wait_for_job (job *j);
void do_job_notification (void);
void continue_job (job *j, int foreground);
void mark_job_as_running (job *j);


#endif
