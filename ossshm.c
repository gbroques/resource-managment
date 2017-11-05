#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ossshm.h"

/**
 * Allocates shared memory for a simulated clock.
 * 
 * @return The shared memory segment ID
 */
int get_clock_shm(void) {
  int id = shmget(IPC_PRIVATE, sizeof(struct my_clock),
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for clock");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to the clock shared memory segment.
 * 
 * @return A pointer to the clock in shared memory.
 */
struct my_clock* attach_to_clock_shm(int id) {
  void* clock_shm = shmat(id, NULL, 0);

  if (*((int*) clock_shm) == -1) {
    perror("Failed to attach to clock shared memory");
    exit(EXIT_FAILURE);
  }

  return (struct my_clock*) clock_shm;
}

/**
 * Detaches from clock shared memory.
 * 
 * @param Clock shared memory
 * @return On success, 0. On error -1.
 */
int detach_from_clock_shm(struct my_clock* shm) {
  int return_value = shmdt(shm);
  if (return_value == -1) {
    perror("Failed to detach from clock shared memory");
  }
  return return_value;
}