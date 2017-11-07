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


/*---------*
 | GLOBALS |
 *---------*/
struct my_clock* clock_shm = NULL;
struct res_node* res_list = NULL;
struct proc_node* proc_list = NULL;

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

/**
 * Get a time to check if the process
 * should terminate, 1 to 250 milliseconds in
 * the future.
 */
struct my_clock get_time_to_check() {
  struct my_clock check_time;
  check_time.secs     = clock_shm->secs;
  check_time.nanosecs = clock_shm->nanosecs;
  int time_to_check = (rand() % 250 + 1) * NANOSECS_PER_MILLISEC;  // 1 to 250 milliseconds
  check_time = add_nanosecs_to_clock(check_time, time_to_check);
  return check_time;
}

static void detach_from_shm() {
  // TODO:
  // Deallocate resources by communicating to master that
  // it's releasing all of those resources

  if (clock_shm != NULL)
    detach_from_clock_shm(clock_shm);
  if (res_list != NULL)
    detach_from_res_list(res_list);
  if (proc_list != NULL)
    detach_from_proc_list(proc_list);
}

static int is_past_time(struct my_clock myclock) {
  return (clock_shm->secs     >= myclock.secs &&
          clock_shm->nanosecs >= myclock.nanosecs);
}

int main(int argc, char* argv[]) {
  if (argc != 7) {
    fprintf(
      stderr,
      "Usage: %s pid bound clock_seg_id res_list_id proc_list_id sem_id\n",
      argv[0]
    );
    return EXIT_FAILURE;
  }

  srand(time(NULL));

  const int pid           = atoi(argv[1]);
  const int bound         = atoi(argv[2]);
  const int clock_id      = atoi(argv[3]);
  const int res_list_id   = atoi(argv[4]);
  const int proc_list_id  = atoi(argv[5]);
  const int sem_id        = atoi(argv[6]);

  signal(SIGTERM, detach_from_shm);

  clock_shm = attach_to_clock_shm(clock_id);
  res_list = attach_to_res_list(res_list_id);
  proc_list = attach_to_proc_list(proc_list_id);

  int is_terminating = 0;

  struct my_clock check_time = get_time_to_check();
  while (!is_terminating) {

    // Every 1 to 250 ms, check should terminate
    if (is_past_time(check_time)) {
      is_terminating = should_terminate();
      check_time = get_time_to_check();
    }
  }

  detach_from_shm();

  return EXIT_SUCCESS;
}