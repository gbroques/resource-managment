#ifndef OSS_H
#define OSS_H

/**************
 * STRUCTURES *
 **************/



// =================================================================


/*************
 * CONSTANTS *
 *************/



// =================================================================


/**************
 * PROTOTYPES *
 **************/
static int setup_interrupt(void);
static int setup_interval_timer(int time);
static void free_shm(void);
static void free_shm_and_abort(int s);
static void print_help_message(char* executable_name,
                               char* log_file);
static int is_required_argument(char optopt);
static void print_required_argument_message(char optopt);
static void fork_and_exec_child();

#endif
