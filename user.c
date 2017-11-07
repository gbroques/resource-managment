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

int main(int argc, char* argv[]) {
  if (argc != 7) {
    fprintf(
      stderr,
      "Usage: %s pid bound clock_seg_id res_list_id proc_list_id sem_id\n",
      argv[0]
    );
    return EXIT_FAILURE;
  }

  const int pid           = atoi(argv[1]);
  const int bound         = atoi(argv[2]);
  const int clock_id      = atoi(argv[3]);
  const int res_list_id   = atoi(argv[4]);
  const int proc_list_id  = atoi(argv[5]);
  const int sem_id        = atoi(argv[6]);

  struct my_clock* clock_shm = attach_to_clock_shm(clock_id);
  struct res_node* res_list = attach_to_res_list(res_list_id);
  struct proc_node* proc_list = attach_to_proc_list(proc_list_id);

  detach_from_clock_shm(clock_shm);
  detach_from_res_list(res_list);
  detach_from_proc_list(proc_list);

  return EXIT_SUCCESS;
}