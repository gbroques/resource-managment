#ifndef OSSSHM_H
#define OSSSHM_H

#include "myclock.h"

/*
 * Operating System Simulator Shared Memory
 *-----------------------------------------*/

int get_clock_shm(void);
struct my_clock* attach_to_clock_shm(int id);
int detach_from_clock_shm(struct my_clock* shm);

#endif
