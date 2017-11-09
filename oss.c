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
#include <sys/types.h>
#include <signal.h>
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

/*---------*
 | GLOBALS |
 *---------*/
char* bound = "250";  // in milliseconds

pid_t children[MAX_PROC];

static FILE* fp;

// Shared Memory Globals
static int clock_id;
static struct my_clock* clock_shm;

static int res_list_id;
static struct res_node* res_list;

static int proc_list_id;
static struct proc_node* proc_list;

static int proc_action_id;
static struct proc_action* proc_action_shm;

static int term_pid_id;
static int* term_pid;

// Semaphore for claiming / releasing resources
static int sem_id;

// Semaphore for communicating a process is terminating
static int term_sem_id;

int main(int argc, char* argv[]) {
  int help_flag = 0;
  char* log_file = "oss.out";
  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "hl:b:")) != -1) {
    switch (c) {
      case 'h':
        help_flag = 1;
        break;
      case 'l':
        log_file = optarg;
        break;
      case 'b':
        bound = optarg;
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
    print_help_message(argv[0], log_file, bound);
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

  srand(time(NULL));

  signal(SIGINT, free_shm_and_abort);
  signal(SIGALRM, free_shm_and_abort);

  int num_grants = 0;

  fp = fopen(log_file, "w+");

  if (fp == NULL) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }

  clock_id = get_clock_shm();
  clock_shm = attach_to_clock_shm(clock_id);

  res_list_id = get_res_list(NUM_RES);
  res_list = attach_to_res_list(res_list_id);
  init_res_list(res_list);

  print_res_list(res_list);

  proc_list_id = get_proc_list(MAX_PROC);
  proc_list = attach_to_proc_list(proc_list_id);
  init_proc_list(proc_list);

  proc_action_id = get_proc_action();
  proc_action_shm = attach_to_proc_action(proc_action_id);
  init_proc_action(proc_action_shm);

  term_pid_id = get_int_shm();
  term_pid = attach_to_int_shm(term_pid_id);

  // Initialize to a value not equal to a PID
  reset_term_pid(term_pid);

  sem_id = allocate_sem(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  init_sem(sem_id, 1);

  term_sem_id = allocate_sem(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  init_sem(term_sem_id, 1);

  // Initialize clock to 1 second to simulate overhead
  clock_shm->secs = 1;

  fork_and_exec_child(0);

  while (1) {
    if (clock_shm->nanosecs % 100000000 == 0) {
      fprintf(stderr, "[%02d:%010d]\n", clock_shm->secs, clock_shm->nanosecs);
    }
    if (clock_shm->nanosecs >= NANOSECS_PER_SEC) {
      clock_shm->secs += 1;
      clock_shm->nanosecs = 0;
    } else {
      // Add 4 for more realistic clock
      clock_shm->nanosecs += 4;
    }

    // Check for resource requests and releases
    if (is_proc_action_available(proc_action_shm)) {
      struct proc_node* proc = proc_list + proc_action_shm->pid;
      struct res_node* res = res_list + proc_action_shm->res_type;
      enum res_action action = proc_action_shm->action;
      char* action_str = action == REQUEST ? "claim" : "release";

      proc->request = res->type;

      fprintf(stderr,
              "[%02d:%010d] Detected P%02d request to %s R%02d\n",
              clock_shm->secs,
              clock_shm->nanosecs,
              proc->id,
              action_str,
              res->type);

      clock_shm->nanosecs += 4;

      // Grant requests to claim or release resources
      if (action == REQUEST && can_grant_request(res->type)) {
        fprintf(stderr,
                "[%02d:%010d] Granting P%02d request for R%02d\n",
                clock_shm->secs,
                clock_shm->nanosecs,
                proc->id,
                res->type);
        num_grants++;
        int i = get_res_instance(res);
        proc->request = -1;
        res->num_allocated++;
        res->held_by[i] = proc->id;
        proc->holds[i] = res->type;
        if (num_grants % 20 == 0 && num_grants != 0) {
          print_res_alloc_table();
        }
      } else if (action == RELEASE && has_resource(proc->id)) {
        fprintf(stderr,
                "[%02d:%010d] Granting P%02d request to release R%02d\n",
                clock_shm->secs,
                clock_shm->nanosecs,
                proc->id,
                res->type);
        int i = 0;
        while (proc->holds[i] != -1) {
          i++;
        }
        res->num_allocated--;
        res->held_by[--i] = -1;
        proc->holds[i] = -1;
      } else if ((res->num_instances - res->num_allocated) == 0) {
        fprintf(stderr,
                "[%02d:%010d] P%02d deadlocked waiting for R%02d\n",
                clock_shm->secs,
                clock_shm->nanosecs,
                proc->id,
                res->type);
      }

      clock_shm->nanosecs += 4;

      fprintf(stderr,
              "[%02d:%010d] %d instance(s) of R%02d left\n",
              clock_shm->secs,
              clock_shm->nanosecs,
              (res->num_instances - res->num_allocated),
              res->type);

      // Reset process action
      init_proc_action(proc_action_shm);
    }

    // Check for terminating processes
    if (*term_pid != -10) {
      // Release all resources held by process
      release_res(*term_pid);
      reset_term_pid(term_pid);  // Set back to -10
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

  detach_from_proc_action(proc_action_shm);
  shmctl(proc_action_id, IPC_RMID, 0);

  detach_from_int_shm(term_pid);
  shmctl(term_pid_id, IPC_RMID, 0);

  deallocate_sem(sem_id);

  deallocate_sem(term_sem_id);
}

/**
 * Free shared memory and abort program
 */
static void free_shm_and_abort(int s) {
  free_shm();
  kill_children();
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
                               char* log_file,
                               char* bound) {
  printf("Operating System Simulator\n\n");
  printf("Usage: ./%s\n\n", executable_name);
  printf("Arguments:\n");
  printf(" -h  Show help.\n");
  printf(" -l  Specify the log file. Defaults to '%s'.\n", log_file);
  printf(" -b  Specify the upper bound for when processes should request or release a resource.\n");
  printf("     Defaults to %s milliseconds.\n", bound);
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
    case 'b':
      fprintf(stderr,
              "Option -%c requires the bound of when a child process will request or let go of a resource.\n",
              optopt);
      break;
    case 'l':
      fprintf(stderr,
              "Option -%c requires the name of the log file.\n",
              optopt);
      break;
  }
}


/**
 * Forks and execs a child process.
 * 
 * @param index Index of children PID array
 */
static void fork_and_exec_child(int index) {
  children[index] = fork();

  if (children[index] == -1) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  if (children[index] == 0) {  // Child
    char num_res_str[12];
    snprintf(num_res_str,
             sizeof(num_res_str),
             "%d",
             NUM_RES);

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

    char proc_action_id_str[12];
    snprintf(proc_action_id_str,
             sizeof(proc_action_id_str),
             "%d",
             proc_action_id);

    char term_pid_id_str[12];
    snprintf(term_pid_id_str,
             sizeof(term_pid_id_str),
             "%d",
             term_pid_id);

    char sem_id_str[12];
    snprintf(sem_id_str,
             sizeof(sem_id_str),
             "%d",
             sem_id);

    char term_sem_id_str[12];
    snprintf(term_sem_id_str,
             sizeof(term_sem_id_str),
             "%d",
             term_sem_id);

    execlp("user",
           "user",
           "0",
           bound,
           num_res_str,
           clock_id_str,
           res_list_id_str,
           proc_list_id_str,
           proc_action_id_str,
           term_pid_id_str,
           sem_id_str,
           term_sem_id_str,
           (char*) NULL);
    perror("Failed to exec");
    _exit(EXIT_FAILURE);
  }
}

/**
 * Initializes the resource list with:
 *   - 1 to 10 instances per resource
 *   - First 3 to 5 resources are shareable
 */
static void init_res_list(struct res_node* res_list) {
  int num_shareable = (rand() % 3) + 3;  // 3 to 5
  int i = 0;
  for (; i < NUM_RES; i++) {
    // Assign 1 to 10 instances per resource
    (res_list + i)->num_instances = rand() % MAX_INSTANCES + 1;

    (res_list + i)->num_allocated = 0;

    (res_list + i)->type = i;

    // Assign first 3 to 5 resources as shareable
    int is_shareable = num_shareable > 0 ? 1 : 0;
    (res_list + i)->shareable = is_shareable;
    num_shareable--;

    int j = 0;
    for (; j < MAX_INSTANCES; j++) {
      (res_list + i)->held_by[j] = -1;
    }
  }
}

/**
 * Initializes the process list with:
 *   - Process ID
 *   - Request set to NULL
 */
static void init_proc_list(struct proc_node* proc_list) {
  int i = 0;
  for (; i < MAX_PROC; i++) {
    proc_list[i].id = i;
    proc_list[i].request = -1;
    int j = 0;
    for (; j < MAX_HOLDS; j++) {
      proc_list[i].holds[j] = -1;
    }
  }
}

static void print_res_list(struct res_node* res_list) {
  int i = 0;
  for (; i < NUM_RES; i++) {
    print_res_node(*(res_list + i));
  }
}

static void print_res_node(struct res_node node) {
  printf("R%02d ", node.type);
  printf("num_instances %02d ", node.num_instances);
  printf("num_allocated %02d ", node.num_allocated);
  printf("shareable %d ", node.shareable);
  int i = 0;
  for (; i < MAX_INSTANCES; i++) {
    if (node.held_by[i] != -1) {
      printf("held by P%02d",
             node.held_by[i]);
    }
  }
  printf("\n");
}

static void kill_children() {
  int i = 0;
  for (; i < MAX_PROC; i++) {
    kill(children[i], SIGKILL);
  }
}

static int can_grant_request(int request) {
  struct res_node* res = (res_list + request);
  return res->num_instances - res->num_allocated;
}

static void init_proc_action(struct proc_action* pa) {
  pa->pid = -10;
  pa->res_type = -10;
  pa->action = IDLE;
}

static int is_proc_action_available(struct proc_action* pa) {
  return pa->pid != -10 && pa->res_type != -10 && pa->action != IDLE;
}

static int has_resource(int pid) {
  return (proc_list + pid)->holds[0] != -1 ? 1 : 0;
}

/**
 * Print resource allocation table
 */
static void print_res_alloc_table(void) {
  // Print header row
  fprintf(stderr, "    ");
  int i = 0;
  for (; i < NUM_RES; i++)
    fprintf(stderr, "R%02d ", i);
  fprintf(stderr, "\n");

  // Print how many resources each process holds
  i = 0;
  for(; i < MAX_PROC; i++) {
    fprintf(stderr, "P%02d  ", i);
    int j = 0;
    for (; j < NUM_RES; j++) {
      int amt = 0;
      int k = 0;
      for (; k < MAX_INSTANCES; k++) {
        if ((res_list + j)->held_by[k] == i)
          amt++;
      }
      fprintf(stderr, "%02d  ", amt);
    }
    fprintf(stderr, "\n");
  }
}


/**
 * Resets the value of the terminating PID to -10
 *
 * @param[in] Pointer to terminating PID
 */
static void reset_term_pid(int* term_pid) {
  *term_pid = -10;
}

/**
 * Releases all resources for a given PID.
 *
 * @param pid The ID of the process
 */
static void release_res(int pid) {
  int i = 0;
  for (; i < NUM_RES; i++) {
    int j = 0;
    for (; j < MAX_INSTANCES; j++) {
      struct res_node* res = res_list + i;
      if (res->held_by[j] == pid)
        res->held_by[j] = -1;
    }
  }
  struct proc_node* proc = proc_list + pid;
  int k = 0;
  while (proc->holds[k] != -1) {
    proc->holds[k] = -1;
    k++;
  }
}