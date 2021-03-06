#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <time.h>
#include "ossshm.h"
#include "resource.h"
#include "myclock.h"
#include "sem.h"

/*-----------------------*
 | Shared Memory Globals |
 *-----------------------*/
struct my_clock* clock_shm          = NULL;
struct res_node* res_list           = NULL;
struct proc_node* proc_list         = NULL;
struct proc_action* proc_action_shm = NULL;
int* term_pid                       = NULL;

// Globals
int pid = -20;
int term_sem_id = -20;
int term_pid_id = -20;

static int should_terminate() {
  int should_terminate;
  int tries = 3;
  int i = 0;

  do {
    should_terminate = rand() % 2;
    i++;
  } while (should_terminate == 1 && i < tries);

  return should_terminate;
}

static int past_initialization() {
  return (
    pid != -20 &&
    term_sem_id != -20 &&
    term_pid_id != -20
  );
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
 * Get a time to check if the process
 * should terminate, 1 to 250 milliseconds in
 * the future.
 */
struct my_clock get_time_to_check() {
  struct my_clock check_time;
  check_time.secs     = clock_shm->secs;
  check_time.nanosecs = clock_shm->nanosecs;
  int time_to_check = get_rand_millisecs(250);
  check_time = add_nanosecs_to_clock(check_time, time_to_check);
  return check_time;
}

/**
 * Get a random time from 1 to bound inclusive,
 * in the future.
 *
 * @param bound The maximum bound in milliseconds
 */
struct my_clock get_rand_future_time(int bound) {
  struct my_clock future_time;
  future_time.secs     = clock_shm->secs;
  future_time.nanosecs = clock_shm->nanosecs;
  int rand_ms = get_rand_millisecs(bound);
  future_time = add_nanosecs_to_clock(future_time, rand_ms);
  return future_time;
}

static void detach_from_shm() {
  if (past_initialization()) {
    // Communicate to OSS to release all resources
    sem_wait(term_sem_id);
      *term_pid = pid;
      // Wait until OSS releases resources
      while (*term_pid != -10);
    sem_post(term_pid_id);
  }

  if (clock_shm != NULL)
    detach_from_clock_shm(clock_shm);
  if (res_list != NULL)
    detach_from_res_list(res_list);
  if (proc_list != NULL)
    detach_from_proc_list(proc_list);
  if (proc_action_shm != NULL)
    detach_from_proc_action(proc_action_shm);
  if (term_pid != NULL)
    detach_from_int_shm(term_pid);
}

static int is_past_time(struct my_clock myclock) {
  return (clock_shm->secs     >= myclock.secs &&
          clock_shm->nanosecs >= myclock.nanosecs);
}

int has_resource(int pid) {
  return (proc_list + pid)->holds[0] != -1 ? 1 : 0;
}

static void request_res(int pid, int num_res) {
  int i = rand() % num_res;
  struct res_node* res = res_list + i;

  // fprintf(stderr, "P%d requesting R%d\n", pid, res->type);
  
  // Make request
  proc_action_shm->pid = pid;
  proc_action_shm->res_type = res->type;
  proc_action_shm->action = REQUEST;

  int num_available = res->num_instances - res->num_allocated;
  if (num_available == 0) {
    // fprintf(stderr, "P%02d waiting. No more of R%02d left.\n", pid, res->type);

    while (1); // Wait if no more instances are available
  }


  // Find next available index
  int k = get_res_instance(res);

  if (k == -1) {  // Exceeded held_by array
    fprintf(stderr, "R%02d already held by 10 resources\n", res->type);
    detach_from_shm();
    exit(EXIT_FAILURE);
  }

  // Wait until request is granted
  while (res->held_by[k] == -1);
}

/**
 * Release the last request resource
 *
 * @param pid The ID of the process releasing the resource
 */
static void release_res(int pid) {
  if (has_resource(pid)) {
    struct proc_node* proc = proc_list + pid;
    int i = 0;
    while (proc->holds[i] != -1) {
      i++;
    }
    i--;

    // Make request
    proc_action_shm->pid = pid;
    proc_action_shm->res_type = proc->holds[i];
    proc_action_shm->action = RELEASE;

    // Wait until request is granted
    while (proc->holds[i] != -1);

  }
}

int main(int argc, char* argv[]) {
  // TODO: Reduce the number of args by putting them into a struct
  if (argc != 11) {
    fprintf(stderr, "Invalid number of arguments\n");
    return EXIT_FAILURE;
  }

  srand(time(NULL));

  pid                       = atoi(argv[1]);
  const int bound           = atoi(argv[2]);
  const int num_res         = atoi(argv[3]);
  const int clock_id        = atoi(argv[4]);
  const int res_list_id     = atoi(argv[5]);
  const int proc_list_id    = atoi(argv[6]);
  const int proc_action_id  = atoi(argv[7]);
  term_pid_id               = atoi(argv[8]);
  const int sem_id          = atoi(argv[9]);
  term_sem_id               = atoi(argv[10]);

  signal(SIGTERM, detach_from_shm);

  clock_shm = attach_to_clock_shm(clock_id);
  res_list = attach_to_res_list(res_list_id);
  proc_list = attach_to_proc_list(proc_list_id);
  proc_action_shm = attach_to_proc_action(proc_action_id);
  term_pid = attach_to_int_shm(term_pid_id);

  // When should process request / release a resource
  struct my_clock res_time = get_rand_future_time(bound);

  int is_terminating = 0;

  struct my_clock check_time = get_rand_future_time(250);
  while (!is_terminating) {

    // Every 1 to bound ms, check should request /
    // release a resource
    if (is_past_time(res_time)) {
      sem_wait(sem_id);
        int action = rand() % 2;
        if (action == 1 && has_resource(pid)) {
          release_res(pid);
        } else {
          request_res(pid, num_res);
        }
      sem_post(sem_id);
      res_time = get_rand_future_time(bound);
    }

    // Every 1 to 250 ms, check should terminate
    if (is_past_time(check_time)) {
      is_terminating = should_terminate();
      check_time = get_rand_future_time(250);
    }
  }

  detach_from_shm();

  return EXIT_SUCCESS;
}