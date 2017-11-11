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
#define MAX_PIDS 256
#define MAX_RUN_TIME 2  // in seconds

/*---------*
 | GLOBALS |
 *---------*/
char* bound = "50";  // in milliseconds

pid_t children[MAX_PIDS];

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

static int num_procs = 0;

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

  proc_list_id = get_proc_list(MAX_PIDS);
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

  // Initialize children PIDs to -10
  int k = 0;
  for (; k < MAX_PIDS; k++)
    children[k] = -10;

  fork_and_exec_child(0);

  struct my_clock fork_time = get_time_to_fork();
  // struct my_clock dd_time = get_time_to_detect_deadlock(atoi(bound));

  while (1) {
    increment_clock();

    if (is_past_time(fork_time)) {
      int pid = get_next_available_pid();
      if (pid != -10 && num_procs < MAX_PIDS) {
        fork_and_exec_child(pid);
        fork_time = get_time_to_fork();
      }
    }

    // if (is_past_time(dd_time)) {
    //   detect_deadlock();
    //   dd_time = get_time_to_detect_deadlock(atoi(bound));
    // }

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

      increment_clock();

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
        detect_deadlock(res->type, proc->id);
      }

      increment_clock();

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
      int released_res[NUM_RES];
      release_res(*term_pid, released_res, NUM_RES);
      fprintf(stderr,
              "[%02d:%010d] Detected P%02d is terminating\n",
              clock_shm->secs,
              clock_shm->nanosecs,
              *term_pid);
      print_released_res(released_res, NUM_RES);
      kill_child(*term_pid);
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
    case 'b':
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
  num_procs++;
  children[index] = fork();

  if (children[index] == -1) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  if (children[index] == 0) {  // Child
    char pid_str[12];
    snprintf(pid_str,
             sizeof(pid_str),
             "%d",
             index);

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
           pid_str,
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
  for (; i < MAX_PIDS; i++) {
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

/**
 * Kills all remaining children
 */
static void kill_children() {
  int i = 0;
  for (; i < MAX_PIDS; i++)
    if (children[i] != -10) {
      kill(children[i], SIGKILL);
      num_procs--;
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
  fprintf(stderr, "\n    ");
  int i = 0;
  for (; i < NUM_RES; i++)
    fprintf(stderr, "R%02d ", i);
  fprintf(stderr, "\n");

  i = 0;
  for (; i < ((NUM_RES + 1) * 4); i++)
    fprintf(stderr, "-");
  fprintf(stderr, "\n");

  // Print how many resources each process holds
  i = 0;
  for(; i < MAX_PIDS; i++) {
    if (children[i] != -10 && children[i] != -20) {
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
  fprintf(stderr, "\n");
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
static void release_res(int pid,
                        int* released_res,
                        int num_res) {
  int i = 0;
  for (; i < num_res; i++) {
    released_res[i] = 0;
    int j = 0;
    for (; j < MAX_INSTANCES; j++) {
      struct res_node* res = res_list + i;
      if (res->held_by[j] == pid) {
        res->held_by[j] = -1;
        released_res[i]++;
        res->num_allocated--;
      }
      increment_clock();
    }
    increment_clock();
  }
  struct proc_node* proc = proc_list + pid;
  int k = 0;
  while (proc->holds[k] != -1 && k < MAX_INSTANCES) {
    proc->holds[k] = -1;
    k++;
    increment_clock();
  }
}

static int is_past_time(struct my_clock myclock) {
  return (clock_shm->secs     >= myclock.secs &&
          clock_shm->nanosecs >= myclock.nanosecs);
}

/**
 * Get a random amount of milliseconds
 *
 * @param bound Maximum bound in millisecons
 * @return Random amount of milliseconds in nanoseconds
 */
int get_rand_millisecs(int bound) {
  return (rand() % bound + 1) * NANOSECS_PER_MILLISEC;
}

/**
 * Get a time to fork a new child process,
 * 1 to 250 milliseconds into the future.
 */
struct my_clock get_time_to_fork() {
  struct my_clock fork_time;
  fork_time.secs     = clock_shm->secs;
  fork_time.nanosecs = clock_shm->nanosecs;
  int time_to_fork = get_rand_millisecs(250);
  fork_time = add_nanosecs_to_clock(fork_time, time_to_fork);
  return fork_time;
}

static int get_next_available_pid() {
  int i = 0;
  while (children[i++] != -10)
    increment_clock();
  
  if (i == MAX_PIDS) {
    fprintf(stderr, "MAXIMUM AMOUNT OF PROCESSES GENERATED ==== !!!!\n");
    return -10;
  }
  
  return --i;
}

/**
 * Get time to run deadlock detection algorithm
 *
 * @param bound
 * @return Time to run deadlock detection algorithm
 */
struct my_clock get_time_to_detect_deadlock(int bound) {
  struct my_clock dd_time;
  dd_time.secs     = clock_shm->secs;
  dd_time.nanosecs = clock_shm->nanosecs;
  int time_to_run = (bound / 2) * (MAX_INSTANCES / 2);
  time_to_run *= NANOSECS_PER_MILLISEC;
  dd_time = add_nanosecs_to_clock(dd_time, time_to_run);
  return dd_time;
}

static void detect_deadlock(int res_type, int pid) {
  fprintf(stderr,
          "[%02d:%010d] Running deadlock detection algorithm...\n",
          clock_shm->secs,
          clock_shm->nanosecs);

  int i = 0;
  struct res_node* res = res_list + res_type;
  fprintf(stderr, "  Processes ");
  for (; i < MAX_INSTANCES; i++)
      if (res->held_by[i] != -1)
        fprintf(stderr, "P%02d ", res->held_by[i]);
  fprintf(stderr, "deadlocked\n");

  fprintf(stderr, "  Attempting to resolve deadlock...\n");
  fprintf(stderr, "  Killing P%d:\n", pid);
  int released_res[NUM_RES];
  release_res(pid, released_res, NUM_RES);
  print_released_res(released_res, NUM_RES);
  kill_child(pid);
  fprintf(stderr, "  System is no longer in deadlock\n");
}

static void increment_clock() {
  clock_shm->nanosecs += 50;
  if (clock_shm->nanosecs >= NANOSECS_PER_SEC) {
    clock_shm->secs += 1;
    clock_shm->nanosecs = 0;
  }
}

static void kill_child(int pid) {
    kill(children[pid], SIGKILL);
    children[pid] = -20;
}

static void print_released_res(int* released_res, int num_res) {
  int has_released_res = 0;
  int i = 0;
  for (; i < num_res; i++)
    if (released_res[i] != 0)
      has_released_res = 1;

  if (has_released_res) {
    fprintf(stderr,
            "    Resources released are as follows: ");
    i = 0;
    for (; i < num_res; i++) {
      if (released_res[i] != 0) {
        fprintf(stderr,
                "R%02d:%d ",
                i,
                released_res[i]);
      }
    }
    fprintf(stderr, "\n");
  }
}