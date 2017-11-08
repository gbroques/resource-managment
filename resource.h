#ifndef RESOURCE_H
#define RESOURCE_H

#define MAX_INSTANCES 10

struct res_node {
  unsigned int type;
  unsigned int num_instances;
  unsigned int num_allocated;
  int shareable;
  int held_by[MAX_INSTANCES];
};

struct proc_node {
  unsigned int id;
  int request;
};

enum res_action {
  IDLE,     // No action to be taken
  REQUEST,  // Request the resource
  RELEASE   // Release the resource
};

/**
 * A structure containing information about the
 * action a process can take upon a resource.
 */
struct proc_action {
  unsigned int pid;
  unsigned int res_type;
  enum res_action action;
};

int get_res_instance(struct res_node* res);

#endif