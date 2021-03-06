#ifndef OSSSHM_H
#define OSSSHM_H

#include "myclock.h"
#include "resource.h"

/*
 * Operating System Simulator Shared Memory
 *-----------------------------------------*/

int get_clock_shm(void);
struct my_clock* attach_to_clock_shm(int id);
int detach_from_clock_shm(struct my_clock* shm);

int get_res_list(int num_res);
struct res_node* attach_to_res_list(int id);
int detach_from_res_list(struct res_node* shm);

int get_proc_list(int num_proc);
struct proc_node* attach_to_proc_list(int id);
int detach_from_proc_list(struct proc_node* shm);

int get_proc_action();
struct proc_action* attach_to_proc_action(int id);
int detach_from_proc_action(struct proc_action* shm);

int get_int_shm();
int* attach_to_int_shm(int id);
int detach_from_int_shm(int* shm);


#endif
