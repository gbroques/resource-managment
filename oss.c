/**
 * Operating System Simulator
 *
 * Copyright (c) 2017 G Brenden Roques
 */

#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <sys/queue.h>
#include "oss.h"
#include "ossshm.h"
#include "myclock.h"
#include "sem.h"
#include "resource.h"

#define NUM_RES 20
#define MAX_PROC 18
#define MAX_RUN_TIME 2  // in seconds

static int clock_id;
static struct my_clock* clock_shm;

static int res_list_id;
static struct res_node* res_list;

static int proc_list_id;
static struct proc_node* proc_list;

static int sem_id;

int main(int argc, char* argv[]) {
  int help_flag = 0;
  char* log_file = "oss.out";
  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "hl:")) != -1) {
    switch (c) {
      case 'h':
        help_flag = 1;
        break;
      case 'l':
        log_file = optarg;
        break;
      case '?':
        if (is_required_argument(optopt)) {
          print_required_argument_message(optopt);
        } else if (isprint(optopt)) {
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        return EXIT_FAILURE;
      default:
        abort();
    }
  }

  if (help_flag) {
    print_help_message(argv[0], log_file);
    exit(EXIT_SUCCESS);
  }

  if (setup_interrupt() == -1) {
    perror("Failed to set up handler for SIGPROF");
    return EXIT_FAILURE;
  }

  if (setup_interval_timer(2) == -1) {
    perror("Faled to set up interval timer");
    return EXIT_FAILURE;
  }

  signal(SIGINT, free_shm_and_abort);
  signal(SIGALRM, free_shm_and_abort);

  clock_id = get_clock_shm();
  clock_shm = attach_to_clock_shm(clock_id);

  res_list_id = get_res_list(NUM_RES);
  res_list = attach_to_res_list(res_list_id);

  proc_list_id = get_proc_list(MAX_PROC);
  proc_list = attach_to_proc_list(proc_list_id);

  sem_id = allocate_sem(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  // Initialize clock to 1 second to simulate overhead
  clock_shm->secs = 1;

  while (1) {
    if (clock_shm->nanosecs >= NANOSECS_PER_SEC) {
      clock_shm->secs += 1;
      clock_shm->nanosecs = 0;
    } else {
      // Add 4 for more realistic clock
      clock_shm->nanosecs += 4;
    }
  }


  free_shm();

  return EXIT_SUCCESS;
}

/**
 * Frees all allocated shared memory
 */
static void free_shm(void) {
  detach_from_clock_shm(clock_shm);
  shmctl(clock_id, IPC_RMID, 0);

  detach_from_res_list(res_list);
  shmctl(res_list_id, IPC_RMID, 0);

  detach_from_proc_list(proc_list);
  shmctl(proc_list_id, IPC_RMID, 0);

  deallocate_sem(sem_id);
}

/**
 * Free shared memory and abort program
 */
static void free_shm_and_abort(int s) {
  free_shm();
  abort();
}


/**
 * Set up the interrupt handler.
 */
static int setup_interrupt(void) {
  struct sigaction act;
  act.sa_handler = free_shm_and_abort;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

/*
 * Sets up an interval timer
 *
 * @param time The duration of the timer in seconds
 *
 * @return Zero on success. -1 on error.
 */
static int setup_interval_timer(int time) {
  struct itimerval value;
  value.it_interval.tv_sec = time;
  value.it_interval.tv_usec = 0;
  value.it_value = value.it_interval;
  return (setitimer(ITIMER_REAL, &value, NULL));
}

/**
 * Prints a help message.
 * The parameters correspond to program arguments.
 *
 * @param executable_name Name of the executable
 * @param log_file Name of the log file
 */
static void print_help_message(char* executable_name,
                               char* log_file) {
  printf("Operating System Simulator\n\n");
  printf("Usage: ./%s\n\n", executable_name);
  printf("Arguments:\n");
  printf(" -h  Show help.\n");
  printf(" -l  Specify the log file. Defaults to '%s'.\n", log_file);
}

/**
 * Determiens whether optopt is a reguired argument.
 *
 * @param optopt Value returned by getopt when there's a missing option argument.
 * @return 1 when optopt is a required argument and 0 otherwise.
 */
static int is_required_argument(char optopt) {
  switch (optopt) {
    case 'l':
      return 1;
    default:
      return 0;
  }
}

/**
 * Prints a message for a missing option argument.
 * 
 * @param option The option that was missing a required argument.
 */
static void print_required_argument_message(char option) {
  switch (option) {
    case 'l':
      fprintf(
        stderr,
        "Option -%c requires the name of the log file.\n",
        optopt);
      break;
  }
}


static void fork_and_exec_child() {
  pid_t pid = fork();

  if (pid == -1) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {  // Child
    char clock_id_str[12];
    snprintf(clock_id_str,
             sizeof(clock_id_str),
             "%d",
             clock_id);

    char res_list_id_str[12];
    snprintf(res_list_id_str,
             sizeof(res_list_id_str),
             "%d",
             res_list_id);

    char proc_list_id_str[12];
    snprintf(proc_list_id_str,
             sizeof(proc_list_id_str),
             "%d",
             proc_list_id);

    char sem_id_str[12];
    snprintf(sem_id_str,
             sizeof(sem_id_str),
             "%d",
             sem_id);

    execlp("user",
           "user",
           "0",
           clock_id_str,
           res_list_id_str,
           proc_list_id_str,
           sem_id_str,
           (char*) NULL);
    perror("Failed to exec");
    _exit(EXIT_FAILURE);
  }
}
