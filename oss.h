#ifndef OSS_H
#define OSS_H

#include "resource.h"
#include "myclock.h"

static int setup_interrupt(void);
static int setup_interval_timer(int time);
static void free_shm(void);
static void free_shm_and_abort(int s);
static void print_help_message(char* executable_name,
                               char* log_file,
                               char* bound);
static int is_required_argument(char optopt);
static void print_required_argument_message(char optopt);
static void fork_and_exec_child();
static void init_res_list(struct res_node* res_list);
static void print_res_list(struct res_node* res_list);
static void print_res_node(struct res_node node);
static void init_proc_list(struct proc_node* proc_list);
static void init_proc_action(struct proc_action* pa);
static void kill_children();
static int can_grant_request(int request);
static int is_proc_action_available(struct proc_action* pa);
static int has_resource(int pid);
static void print_res_alloc_table(void);
static void reset_term_pid(int* term_pid);
static void release_res(int pid, int* released_res, int num_res);
static int is_past_time(struct my_clock myclock);
int get_rand_millisecs(int bound);
struct my_clock get_time_to_fork();
static int get_next_available_pid();
struct my_clock get_time_to_detect_deadlock(int bound);
static void detect_deadlock(int res_type, int pid);
static void increment_clock(void);
static void kill_child(int pid);
static void print_released_res(int* released_res, int num_res);

#endif
