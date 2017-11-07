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

static void detach_from_shm() {
  if (clock_shm != NULL)
    detach_from_clock_shm(clock_shm);
  if (res_list != NULL)
    detach_from_res_list(res_list);
  if (proc_list != NULL)
    detach_from_proc_list(proc_list);
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

  // Check if should terminate in 1 to 250 milliseconds
  struct my_clock check_time;
  check_time.secs     = clock_shm->secs;
  check_time.nanosecs = clock_shm->nanosecs;
  int time_to_check = (rand() % 250 + 1) * NANOSECS_PER_MILLISEC;  // 1 to 250 milliseconds
  check_time = add_nanosecs_to_clock(check_time, time_to_check);
  fprintf(stderr, "[%02d:%010d] START\n", clock_shm->secs, clock_shm->nanosecs);
  fprintf(stderr, "[%02d:%010d] CHECK\n", check_time.secs, check_time.nanosecs);
  while (!is_terminating) {
    if (clock_shm->secs     >= check_time.secs &&
        clock_shm->nanosecs >= check_time.nanosecs) {
      is_terminating = should_terminate();
      fprintf(stderr, "Checking... should terminate %d\n", is_terminating);

      time_to_check = (rand() % 250 + 1) * NANOSECS_PER_MILLISEC;  // 1 to 250 milliseconds
      check_time = add_nanosecs_to_clock(check_time, time_to_check);
    fprintf(stderr, "[%02d:%010d] CHECK AGAIN\n", check_time.secs, check_time.nanosecs);
    }
  }


  // Deallocate resources by communicating to master that
  // it's releasing all of those resources

  detach_from_shm();

  return EXIT_SUCCESS;
}