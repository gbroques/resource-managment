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
#include "myclock.h"

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(
      stderr,
      "Usage: %s pid clock_seg_id\n",
      argv[0]
    );
    return EXIT_FAILURE;
  }

  const int pid = atoi(argv[1]);
  const int clock_seg_id = atoi(argv[2]);

  struct my_clock* clock_shm = attach_to_clock_shm(clock_seg_id);

  detach_from_clock_shm(clock_shm);

  return EXIT_SUCCESS;
}