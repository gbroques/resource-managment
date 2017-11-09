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
  int success = shmdt(shm);
  if (success == -1) {
    perror("Failed to detach from clock shared memory");
  }
  return success;
}


/**
 * Allocates shared memory for a list of resources.
 * 
 * @return The shared memory segment ID
 */
int get_res_list(int num_res) {
  int size = sizeof(struct res_node) * num_res;
  int id = shmget(IPC_PRIVATE, size,
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for resource list");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to resource list shared memory segment.
 * 
 * @return A pointer to a resource list in shared memory.
 */
struct res_node* attach_to_res_list(int id) {
  void* shm = shmat(id, NULL, 0);

  if (*((int*) shm) == -1) {
    perror("Failed to attach to shared memory for resource list");
    exit(EXIT_FAILURE);
  }

  return (struct res_node*) shm;
}

/**
 * Detaches from resource list in shared memory.
 * 
 * @param Resource list in shared memory
 * @return On success, 0. On error -1.
 */
int detach_from_res_list(struct res_node* shm) {
  int success = shmdt(shm);
  if (success == -1) {
    perror("Failed to detach from shared memory for resource list");
  }
  return success;
}


/**
 * Allocates shared memory for process list.
 * 
 * @return The shared memory segment ID
 */
int get_proc_list(int num_procs) {
  int size = sizeof(struct proc_node) * num_procs;
  int id = shmget(IPC_PRIVATE, size,
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for process list");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to process list shared memory segment.
 * 
 * @return A pointer to a list of process nodes in shared memory.
 */
struct proc_node* attach_to_proc_list(int id) {
  void* shm = shmat(id, NULL, 0);

  if (*((int*) shm) == -1) {
    perror("Failed to attach to shared memory for process list");
    exit(EXIT_FAILURE);
  }

  return (struct proc_node*) shm;
}

/**
 * Detaches from process list in shared memory.
 * 
 * @param Process list in shared memory
 * @return On success, 0. On error -1.
 */
int detach_from_proc_list(struct proc_node* shm) {
  int success = shmdt(shm);
  if (success == -1) {
    perror("Failed to detach from process list shared memory");
  }
  return success;
}

/**
 * Allocates shared memory for a process action.
 * 
 * @return The shared memory segment ID
 */
int get_proc_action() {
  int id = shmget(IPC_PRIVATE, sizeof(struct proc_action),
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for process action");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to a process action shared memory segment.
 * 
 * @return A pointer to a process action in shared memory.
 */
struct proc_action* attach_to_proc_action(int id) {
  void* shm = shmat(id, NULL, 0);

  if (*((int*) shm) == -1) {
    perror("Failed to attach to shared memory for process action");
    exit(EXIT_FAILURE);
  }

  return (struct proc_action*) shm;
}

/**
 * Detaches from process action in shared memory.
 * 
 * @param Process action in shared memory
 * @return On success, 0. On error -1.
 */
int detach_from_proc_action(struct proc_action* shm) {
  int success = shmdt(shm);
  if (success == -1) {
    perror("Failed to detach from process action shared memory");
  }
  return success;
}

/**
 * Allocates shared memory for an integer.
 * 
 * @return The shared memory segment ID
 */
int get_int_shm(void) {
  int id = shmget(IPC_PRIVATE, sizeof(int),
    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

  if (id == -1) {
    perror("Failed to get shared memory for int");
    exit(EXIT_FAILURE);
  }
  return id;
}

/**
 * Attaches to an int shared memory segment.
 * 
 * @return A pointer to an int in shared memory.
 */
int* attach_to_int_shm(int id) {
  int* shm = shmat(id, NULL, 0);

  if (*shm == -1) {
    perror("Failed to attach to int shared memory");
    exit(EXIT_FAILURE);
  }

  return shm;
}

/**
 * Detaches from an int in shared memory.
 * 
 * @param Int shared memory
 * @return On success, 0. On error -1.
 */
int detach_from_int_shm(int* shm) {
  int success = shmdt(shm);
  if (success == -1) {
    perror("Failed to detach from int shared memory");
  }
  return success;
}
